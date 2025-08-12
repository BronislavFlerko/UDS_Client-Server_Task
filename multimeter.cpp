#include "multimeter.h"

MultimeterCore::MultimeterCore() : gen(rd()) {
    channels.resize(MAX_CHANNELS);
    ChannelsInit();
    voltage_thread = std::thread(&MultimeterCore::RandomizeVoltage, this);
    state_thread = std::thread(&MultimeterCore::RandomizeChannelState, this);
}

MultimeterCore::~MultimeterCore() {
    running = false;
    if (voltage_thread.joinable()) {
        voltage_thread.join();
    }
    if (state_thread.joinable()) {
        state_thread.join();
    }
}

void MultimeterCore::RandomizeVoltage() {
    while (running) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& channel : channels) {
                if (channel.state == measure_state || channel.state == busy_state) {
                    auto range = channel.ranges_voltage[channel.range];
                    std::uniform_real_distribution<float> dist(range.first, range.second);
                    channel.current_value = dist(gen);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void MultimeterCore::RandomizeChannelState() {
    std::uniform_int_distribution<> check_interval(10, 15);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    const float ERROR = 0.02f;
    const float BUSY = 0.2f;

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(check_interval(gen)));
        {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& channel : channels) {
                if (channel.state == measure_state) {
                    float roll = chance(gen);
                    if (roll < ERROR) {
                        channel.state = error_state;
                    } else if (roll < BUSY) {
                        channel.state = busy_state;
                        // Захватываем this и channel по ссылке
                        std::thread([this, &channel]() {
                            std::this_thread::sleep_for(std::chrono::seconds(10));
                            std::lock_guard<std::mutex> lock(this->mtx);
                            if (channel.state == busy_state) {
                                channel.state = measure_state;
                            }
                        }).detach();
                    }
                }
            }
        }
    }
}

void MultimeterCore::ChannelsInit() {
    for (size_t i = 0; i < channels.size(); ++i) {
        channels[i].name = "channel" + std::to_string(i);
    }
    current_channel_count = channels.size();
}

std::string MultimeterCore::ChannelStateToString(ChannelState state) {
    switch (state) {
    case error_state: return "error_state";
    case idle_state: return "idle_state";
    case measure_state: return "measure_state";
    case busy_state: return "busy_state";
    }
    return "";
}

bool MultimeterCore::isValidChannel(const std::string& channel_par) const {

    if (channel_par.substr(0, 7) != "channel") {
        return false;
    }
    try {
        size_t channel_num = std::stoul(channel_par.substr(7));
        return channel_num < MAX_CHANNELS;
    } catch (...) {
        return false;
    }
}

 /*
bool MultimeterCore::isValidRange(const std::string& range_par) const {

    if (range_par.substr(0, 5) != "range") {
        return false;
    }
    try {
        size_t range_num = std::stoul(range_par.substr(5));
        return range_num >= 0 && range_num <= 3;
    } catch (...) {
        return false;
    }
} */

std::vector<Channel>::iterator MultimeterCore::FindChannelByName(const std::string& channel_par) {
    return std::find_if(channels.begin(), channels.end(),
                        [&channel_par](const Channel& channel) { return channel.name == channel_par; });
}

void MultimeterCore::StartMeasure(const std::string& channel_par, std::ostream& os) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end()) {
        it->state = measure_state;
        os << "ok\r";
    } else {
        os << "fail\r";
    }
}

void MultimeterCore::SetRange(const std::string& channel_par, const std::string& range_par, std::ostream& os) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state == idle_state) {

        size_t range_num = std::stoul(range_par.substr(5));
        it->range = static_cast<Ranges>(range_num);
        os << "ok, " << range_par << "\r";

    } else {
        os << "fail, " << range_par << "\r";
    }
}

void MultimeterCore::StopMeasure(const std::string& channel_par, std::ostream& os) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state != error_state && it->state != busy_state) {
        it->state = idle_state;
        os << "ok\r";
    } else {
        os << "fail\r";
    }
}

void MultimeterCore::GetStatus(const std::string& channel_par, std::ostream& os) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state != error_state) {
        os << "ok, " << ChannelStateToString(it->state) << "\r";
    } else {
        os << "fail, " << ChannelStateToString(it->state) << "\r";
    }
}

void MultimeterCore::GetResult(const std::string& channel_par, std::ostream& os) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state == measure_state) {
        os << "ok, " /*<< std::fixed << std::setprecision(7)*/ << it->current_value << "\r";
    } else {
        os << "fail\r";
    }
}

void MultimeterCore::Diagnostic(const std::string& command, const std::string& channel_par, std::ostream& os) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state == error_state && command == "diagnostic") {
        it->state = idle_state;
        os << "ok, " << channel_par << "\r";
    } else {
        os << "fail, " << channel_par << "\r";
    }
}

std::string MultimeterCore::ProcessCommand(const std::string& input) {
    std::istringstream iss(input);
    std::string command, channel_par, range_par;
    iss >> command;

    // Специальная обработка для set_range в формате "set_range channelX, rangeY"
    if (command == "set_range") {
        std::regex set_range_format(R"(^channel\d+,\srange[0-3]$)");
        std::string params;
        std::getline(iss, params); // Читаем всё после "set_range"

        // Удаляем начальные/конечные пробелы у параметров
        params.erase(0, params.find_first_not_of(" \t"));
        params.erase(params.find_last_not_of(" \t") + 1);
        // Проверяем, что параметры не пустые
        if (params.empty()) {
            std::ostringstream os;
            os << "fail, no parameters\r";
            return os.str();
        }
        size_t comma_pos = params.find(',');
        if (comma_pos == std::string::npos) {
            std::ostringstream os;
            os << "fail, invalid format\r";
            return os.str();
        }
        channel_par = params.substr(0, comma_pos);
        range_par = params.substr(comma_pos + 2); // +2 чтобы пропустить ", "
        if (!std::regex_match(params, set_range_format)) {
            std::ostringstream os;
            os << "fail, " << range_par << "\r";
            return os.str();
        }
    } else { iss >> channel_par; } // Для других команд просто добавляется имя канала

    std::ostringstream response_stream;

    try {

        if (command == "start_measure") {
            if (!isValidChannel(channel_par)) {
                response_stream << "fail\r";
            } else {
                StartMeasure(channel_par, response_stream);
            }
        }
        else if (command == "set_range") {
            SetRange(channel_par, range_par, response_stream);
        }
        else if (command == "stop_measure") {
            if (!isValidChannel(channel_par)) {
                response_stream << "fail\r";
            } else {
                StopMeasure(channel_par, response_stream);
            }
        }
        else if (command == "get_status") {
            if (!isValidChannel(channel_par)) {
                response_stream << "fail\r";
            } else {
                GetStatus(channel_par, response_stream);
            }
        }
        else if (command == "get_result") {
            if (!isValidChannel(channel_par)) {
                response_stream << "fail\r";
            } else {
                GetResult(channel_par, response_stream);
            }
        }
        else if (command == "diagnostic") {
            if (!isValidChannel(channel_par)) {
                response_stream << "fail\r";
            } else {
                Diagnostic(command, channel_par, response_stream);
            }
        }
        else {
            response_stream << "fail, unknown command\r";
        }
    } catch (const std::exception& e) {
        response_stream << "fail, exception - " << e.what() << "\r";
    } catch (...) {
        response_stream << "fail, unknown error\r";
    }

    return response_stream.str();
}

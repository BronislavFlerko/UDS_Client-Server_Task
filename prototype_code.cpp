//Прототипный монолит, основной каркас мультиметра,
//позже будет разбит на хэдеры/реализацию и клиент-сервер ахритектуру.
//служит лишь для демонстрации промежуточного этапа,
//в финальной сборке не используется.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <map>
#include <thread>
#include <chrono>
#include <random>
#include <mutex>
#include <utility>
#include <iomanip>
#include <sys/socket.h>
#include <sys/un.h>


const size_t MAX_CHANNELS = 2;  // Минимум 2 канала (работает с любым количеством)
const size_t NUM_RANGES = 4;    // range0..range3
const std::string SOCKET_PATH = "/tmp/multimeter.sock";
size_t current_channel_count = 0;


enum ChannelState {
    error_state,
    idle_state,
    measure_state,
    busy_state
};

// Функция для преобразования ChannelState в строку
std::string ChannelStateToString(ChannelState state) {
    switch (state) {
    case error_state:
        return "error_state";
    case idle_state:
        return "idle_state";
    case measure_state:
        return "measure_state";
    case busy_state:
        return "busy_state";
    }
    return "";
}


enum Ranges {
    range0,
    range1,
    range2,
    range3
};

struct Channel {
    std::string name;
    ChannelState state = idle_state;
    Ranges range = range0;

    std::map<Ranges, std::pair<float, float>> ranges_voltage = {
        {range0, {0.0000001f, 0.001f}},    // [0.0000001 ... 0.001) В
        {range1, {0.001f, 1.0f}},          // [0.001 ... 1) В
        {range2, {1.0f, 1000.0f}},         // [1 ... 1000) В
        {range3, {1000.0f, 1000000.0f}}    // [1000 ... 1000000) В
    };

    int current_range = 0;
    float current_value = 0.0f;
};

std::vector<Channel> channels(MAX_CHANNELS);
std::mutex mtx; // Мьютекс для защиты доступа к каналам
volatile bool running = true;

std::random_device rd;
std::mt19937 gen(rd());

// Функция для рандомизации напряжения в заданном диапазоне
void RandomizeVoltage() {
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

//Функция, которая вызывает  error/busy_state в случайный момент времени, пока канал в measure_state
void RandomizeChannelState() {

    std::uniform_int_distribution<> check_interval(10, 15); // Проверка каждые 10-15 сек
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    const float ERROR = 0.02f;    // 2% вероятность ошибки
    const float BUSY = 0.2f;      // 20% вероятность занятости

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(check_interval(gen)));

        std::lock_guard<std::mutex> lock(mtx);
        for (auto& channel : channels) {
            if (channel.state == measure_state) {
                float roll = chance(gen);

                if (roll < ERROR) {
                    channel.state = error_state;
                }
                else if (roll < BUSY) {
                    channel.state = busy_state;
                    // Автосброс через 10 секунд
                    std::thread([&channel]() {
                        std::this_thread::sleep_for(std::chrono::seconds(10));
                        std::lock_guard<std::mutex> lock(mtx);
                        if (channel.state == busy_state) {
                            channel.state = measure_state;
                        }
                    }).detach();
                }
            }
        }
    }
}

void ChannelsInit() {
    if(current_channel_count != MAX_CHANNELS) {
        for(size_t i = 0; i < channels.size(); ++i) {
            channels[i].name = "channel" + std::to_string(i);
        }
    }
    current_channel_count = channels.size(); // Обновляем счетчик
}

auto FindChannelByName(const std::string& channel_par) {
    return std::find_if(channels.begin(), channels.end(), [&channel_par](const Channel& channel) {
        return channel.name == channel_par;
    });
}



std::string InputCommand() {

    std::string inputed_command_line;
    getline(std::cin, inputed_command_line);
    return inputed_command_line + "\r";
}

void InvalidInputInfo() { std::cerr << "Неверный формат ввода.\r"
              << "Ожидается валидная команда и минимум один параметр. Пример:\r"
              << "\"set_range channel0, range0\"\r"
              <<"\"get_status channel0\"\r"; }


const std::vector<std::string> valid_command_list = { "start_measure", "set_range", "stop_measure", "get_status", "get_result", "diagnostic" };



void StartMeasure(const std::string& channel_par) {

    auto it = FindChannelByName(channel_par);

    if (it != channels.end()) {
        it->state = measure_state;
        std::cout << "ok\r";
    } else {
        std::cout << "fail\r";
    }
}

void SetRange(const std::string& channel_par, const std::string range_par) {

    auto it = FindChannelByName(channel_par);

    if(it != channels.end() && it->state != error_state && it->state != busy_state) {

        size_t range_num = 0;

        try {
            std::string num(1, range_par[range_par.size() - 1]);
            range_num = std::stoul(num);

        } catch (const std::invalid_argument& e) {
            std::cout << "fail, последний символ диапазона не является числом: " << range_par.back() << "\r";
            return;
        }

        if (range_par.substr(0, 5) != "range" || range_num > 3) {
            std::cout << "fail, " << range_par << "\r";
            return;
        }
        else {

            it->range = static_cast<Ranges>(range_num);
            std::cout << "ok, " << range_par << "\r";

        }
    } else { std::cout << "fail\r"; }
}

void StopMeasure(const std::string& channel_par) {

    auto it = FindChannelByName(channel_par);

    if (it != channels.end() && it->state != error_state && it->state != busy_state) {
        it->state = idle_state;
        std::cout << "ok\r";
    } else {
        std::cout << "fail\r";
    }
}

void GetStatus(const std::string& channel_par) {

    std::lock_guard<std::mutex> lock(mtx);
    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state != error_state) {
        std::cout << "ok, " << ChannelStateToString(it->state) << "\r";
    }
    else {
        std::cout << "fail, " << ChannelStateToString(it->state) << "\r";
    }
}

void GetResult(const std::string& channel_par) {

    auto it = FindChannelByName(channel_par);
    if (it != channels.end() && it->state == measure_state) {

        std::cout << "ok, " << it->current_value << "\r";
    }
    else {
        std::cout << "fail" << "\r";
    }
}

//Функция-команда для явной обработки канала в состоянии error_state со стороны клиента,
//имитирует деятельность, блокируя поток длиной 5 секунд
void Diagnostic(const std::string& command, const std::string& channel_par) {

    auto it = FindChannelByName(channel_par);
    if(it != channels.end() && it->state == error_state && command == "diagnostic") {
        std::cout << "Началась даигностика, пожалуйста подождите...\r";
        std::this_thread::sleep_for(std::chrono::seconds(5));
        it->state = idle_state;
        std::cout << "Диагностика завершена. Канал ожидает указаний для начала работы.\r";
    }
    else {
        std::cout << "Проблем не обнаружено, операция отменена.\r";
    }
}

void FindValidCommand(std::string cmd) {

    auto it = std::find(valid_command_list.begin(), valid_command_list.end(), cmd);
    if (it == valid_command_list.end()) {
        std::cerr << "Команда \"" << cmd << "\" не найдена. Список команд:\r";
        for(const std::string& command : valid_command_list) {
            std::cerr << command << "\r";
        }
        std::cout << std::endl;
    }
}

void IsValidParameter(const std::string& command, std::string& channel_par, std::string& range_par, int max_channels) {
    if (channel_par.empty()) {
        InvalidInputInfo();
        return;
    }

    // Удаляем запятую, если она есть
    if (channel_par.back() == ',') {
        if (command != "set_range") {
            std::cerr << "Отказано. Второй параметр (range) предназначен только для команды set_range.\r";
            if (range_par.empty()) {
                std::cerr << "Пропушен параметр диапазона (range).\r";
            }
            return;
        }
        channel_par.pop_back(); // Удаляем запятую
    }

    // Проверяем формат имени канала
    std::regex channel_regex("^channel(\\d+)$");
    std::smatch matches;

    if (!std::regex_match(channel_par, matches, channel_regex)) {
        std::cerr << "Параметр " << "\"" << channel_par << "\"" << " не соответствует формату \"channel<число>\"\n";
        return;
    }

    // Преобразуем номер канала
    try {
        int channel_num = std::stoi(matches[1].str());
        if (channel_num < 0 || channel_num > max_channels) {
            std::cerr << "Номер канала " << channel_num << " вне допустимого диапазона.\r";
            return;
        }
    } catch (...) {
        std::cerr << "Ошибка преобразования номера канала\r";
        return;
    }

    // Читаем range_par, если это команда set_range
    if (command == "set_range" && range_par.empty()) {
        std::cerr << "Пропушен параметр диапазона (range).\r";
    } /*else {
        std::cout << "Command: " << command
                  << " | Channel: " << channel_par
                  << " | Range: " << range_par << "\r";
    }*/
}

void CommandExecutor(const std::string& command, const std::string& channel_par, const std::string& range_par) {
    if (command == "start_measure") {
        StartMeasure(channel_par);
    } else if (command == "set_range") {
        SetRange(channel_par, range_par);
    } else if (command == "stop_measure") {
        StopMeasure(channel_par);
    } else if (command == "get_status") {
        GetStatus(channel_par);
    } else if (command == "get_result") {
        GetResult(channel_par);
    } else if (command == "diagnostic") {
        Diagnostic(command, channel_par);
    }
}

void ChannelsInfo() {
    std::cout << channels.size() << std::endl;
    for(Channel el : channels) { std::cout << el.name << ' ' << el.state << ' ' << el.current_range << ' ' << el.current_value << std::endl;}
}

//#ifdef SERVER
int main() {

    ChannelsInit();

    ChannelsInfo();

    std::thread voltage_thread(RandomizeVoltage);
    std::thread state_thread(RandomizeChannelState);

    /*
    for(int i = 0; i < 100; ++i) {
        channels[0].state = measure_state;
        std::cout << std::fixed << std::setprecision(7);
        std::cout << channels[0].current_value << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));    }
    */

    for(;;) {


        std::istringstream iss(InputCommand());
        std::string command, channel_par, range_par;
        iss >> command >> channel_par >> range_par;
        FindValidCommand(command);
        IsValidParameter(command, channel_par, range_par, MAX_CHANNELS - 1);
        CommandExecutor(command, channel_par, range_par);

        std::cout << std::endl;
        //std::cout << command << ' ' << channel_par << ' ' << range_par << std::endl;
    }

    voltage_thread.join();
    state_thread.join();

    return 0;
}

/*
#else
int main() {


    return 0;
} */

#pragma once

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

const size_t MAX_CHANNELS = MULTIMETER_CHANNELS;

enum ChannelState {
    error_state,
    idle_state,
    measure_state,
    busy_state
};

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
        {range0, {0.0000001f, 0.001f}},
        {range1, {0.001f, 1.0f}},
        {range2, {1.0f, 1000.0f}},
        {range3, {1000.0f, 1000000.0f}}
    };
    float current_value = 0.0f;
};

class MultimeterCore {
public:
    MultimeterCore();
    ~MultimeterCore();

    void ChannelsInit();
    void RandomizeVoltage();
    void RandomizeChannelState();
    std::string ChannelStateToString(ChannelState state);
    std::string ProcessCommand(const std::string& input);

private:
    std::vector<Channel> channels;
    std::random_device rd;
    std::mt19937 gen;
    volatile bool running = true;
    size_t current_channel_count = 0;
    std::thread voltage_thread;
    std::thread state_thread;
    std::mutex mtx;

    bool isValidChannel(const std::string& channel) const;
    bool isValidRange(const std::string& range) const;
    std::vector<Channel>::iterator FindChannelByName(const std::string& channel_par);
    void StartMeasure(const std::string& channel_par, std::ostream& os);
    void SetRange(const std::string& channel_par, const std::string& range_par, std::ostream& os);
    void StopMeasure(const std::string& channel_par, std::ostream& os);
    void GetStatus(const std::string& channel_par, std::ostream& os);
    void GetResult(const std::string& channel_par, std::ostream& os);
    void Diagnostic(const std::string& command, const std::string& channel_par, std::ostream& os);
};

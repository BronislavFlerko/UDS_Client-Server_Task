#pragma once
#include "multimeter.h"
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <thread> // Для HandleClient

class Server {
public:
    Server(MultimeterCore& core);
    void Run();

private:
    MultimeterCore& core_;
    const std::string socket_path_ = "/tmp/multimeter.sock";

    // Метод для обработки каждого клиентского соединения
    void HandleClient(int client_fd);
};

#pragma once
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

const std::string CLIENT_SOCKET_PATH = "/tmp/multimeter.sock"; // Путь к сокету сервера

class Client {
public:
    Client(); // Конструктор
    ~Client(); // Деструктор

    // Метод для отправки команды по уже установленному соединению
    std::string SendCommand(const std::string& command);

private:
    int sock_fd; // Файловый дескриптор сокета
    bool connected; // Флаг состояния соединения

    // Приватные методы для установки и разрыва соединения
    bool connect_to_server();
    void disconnect_from_server();
};

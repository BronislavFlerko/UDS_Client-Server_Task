// server.cpp
#include "server.h"
#include <string.h> // Для strerror
#include <ctime>    // Для времени в логах

std::string CurrentTime() {
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

Server::Server(MultimeterCore& core) : core_(core) {}

void Server::Run() {
    int server_fd;
    struct sockaddr_un addr;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    unlink(socket_path_.c_str()); // Удаляем сокет, если он уже существует

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "[" << CurrentTime() << "] Сервер запущен. Ожидание подключений на " << socket_path_ << std::endl;

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        std::cout << "[" << CurrentTime() << "] Новый клиент подключен. FD: " << client_fd << std::endl;

        // Запускаем обработку клиента в отдельном потоке
        std::thread([this, client_fd]() { this->HandleClient(client_fd); }).detach();
    }
    close(server_fd); // Закрываем серверный сокет при выходе из цикла
}

void Server::HandleClient(int client_fd) {
    char buffer[1024];
    while (true) {
        int bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            std::cout << "[" << CurrentTime() << "] Клиент " << client_fd << " отключился." << std::endl;
            break;
        }

        std::string command(buffer, bytes_read);
        // Удаляем лишние символы (CR/LF) если они есть
        command.erase(std::remove(command.begin(), command.end(), '\r'), command.end());
        command.erase(std::remove(command.begin(), command.end(), '\n'), command.end());

        // Пропускаем пустые команды
        if (command.empty()) { continue; }

        std::cout << "[" << CurrentTime() << "] Клиент " << client_fd << " отправил команду: " << command << std::endl;

        std::string response = core_.ProcessCommand(command);

        std::cout << "[" << CurrentTime() << "] Отправляем клиенту " << client_fd << " ответ: " << response << std::endl;

        if (write(client_fd, response.c_str(), response.size()) == -1) {
            perror("write");
            break;
        }
    }
    close(client_fd);
}

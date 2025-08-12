#include "client.h"

// Конструктор пытается установить соединение при создании объекта Client
Client::Client() : sock_fd(-1), connected(false) {
    connected = connect_to_server();
    if (!connected) {
        std::cerr << "Не удалось подключиться к серверу при инициализации.\r";
    }
}

// Деструктор закрывает соединение при уничтожении объекта Client
Client::~Client() {
    disconnect_from_server();
}

bool Client::connect_to_server() {
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        std::cerr << "Ошибка создания сокета: " << strerror(errno) << "\r";
        return false;
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIENT_SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "Ошибка подключения к серверу: " << strerror(errno) << "\r";
        close(sock_fd);
        sock_fd = -1;
        return false;
    }
    return true;
}

void Client::disconnect_from_server() {
    if (sock_fd != -1) {
        close(sock_fd);
        sock_fd = -1;
        connected = false;
    }
}

// Метод для отправки команды по установленному соединению
std::string Client::SendCommand(const std::string& command) {
    if (!connected) {
        // Попытаться переподключиться, если соединение было потеряно
        std::cerr << "Соединение потеряно, попытка переподключения...\r";
        connected = connect_to_server();
        if (!connected) {
            return "не удалось восстановить соединение\r";
        }
    }

    std::string cmd_to_send = command + "\r";
    if (write(sock_fd, cmd_to_send.c_str(), cmd_to_send.size()) == -1) {
        std::cerr << "Client: Ошибка отправки команды\r";
        // Если произошла ошибка записи, возможно, соединение разорвано
        disconnect_from_server();
        return "не удалось отправить команду, соединение, возможно, разорвано\r";
    }

    char buffer[1024] = {0};
    ssize_t bytes_read = read(sock_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string response(buffer);
        while (!response.empty() && (response.back() == '\r' || response.back() == '\n')) {
            response.pop_back();
        }
        return response;
    } else if (bytes_read == 0) {
        // Сервер закрыл соединение
        std::cerr << "Client: Сервер закрыл соединение.\r";
        disconnect_from_server();
        return "сервер закрыл соединение\r";
    } else {
        std::cerr << "Client: Ошибка чтения ответа\r";
        // Ошибка чтения, возможно, соединение разорвано
        disconnect_from_server();
        return "ошибка чтения ответа, соединение, возможно, разорвано\r";
    }
}

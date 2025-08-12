#include "multimeter.h"
#include "server.h"
#include "client.h" // Включаем client.h для использования класса Client
#include <iostream>
#include <string>

int main() {
#ifdef SERVER
    MultimeterCore core; // Создаем экземпляр MultimeterCore
    Server server(core); // Передаем его в Server
    server.Run();
#else
    Client client;
    std::string command;
    while (true) {
        std::cout << "> " << std::flush; // Явный сброс буфера
        if (!std::getline(std::cin, command)) {
            break; // Выход при ошибке ввода
        }

        if (command.empty()) {
            std::cout << "fail, empty command line" << std::endl;
            continue;
        }

        if (command == "exit") { break; }

        std::string response = client.SendCommand(command);
        std::cout << response << std::endl; // Вывод ответа с новой строки
    }
#endif
    return 0;
}

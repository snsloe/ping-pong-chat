#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

#define PORT 8080
#define BUFFER_SIZE 1024

std::vector<std::string> messageHistory;
std::mutex historyMutex;

void handleError(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void handleClient(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    const char* pong_message = "pong";

    while (true) {
        memset(buffer, 0, BUFFER_SIZE); // Очищаем буфер
        int valread = read(client_socket, buffer, BUFFER_SIZE);

        if (valread <= 0) {
            std::cerr << "Клиент отключился или произошла ошибка." << std::endl;
            close(client_socket);
            break;
        }

        std::string client_message(buffer, valread);
        client_message.erase(client_message.find_last_not_of("\r\n") + 1);

        if (client_message.empty()) {
            continue;
        }

        std::cout << "Получено: " << client_message << std::endl;

        if (client_message == "ping") {
            {
                std::lock_guard<std::mutex> lock(historyMutex);
                messageHistory.push_back("You: ping");
                messageHistory.push_back("Server: pong");
            }
            send(client_socket, pong_message, strlen(pong_message), 0);
            std::cout << "Отправлено: pong" << std::endl;

        } else if (client_message == "exit") {
            std::cout << "Клиент запросил отключение. Закрытие соединения." << std::endl;
            break;

        } else if (client_message == "history") {
            std::cout << "Клиент запросил историю сообщений." << std::endl;

            std::string history = "Show History...\n";
            {
                std::lock_guard<std::mutex> lock(historyMutex);
                for (const auto& entry : messageHistory) {
                    history += entry + "\n";
                }
            }

            send(client_socket, history.c_str(), history.size(), 0);
        }
        else {
            std::cerr << "Неизвестное сообщение: " << client_message << std::endl;
            std::string response = "Unknown command: " + client_message;

            {
                std::lock_guard<std::mutex> lock(historyMutex);
                messageHistory.push_back("You: " + client_message);  // Сохраняем запрос клиента
                messageHistory.push_back("Server: " + response);    // Сохраняем ответ сервера
            }

            send(client_socket, response.c_str(), response.size(), 0);
        }
    }

    close(client_socket);
}


int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        handleError("Ошибка создания сокета");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        handleError("Ошибка установки параметров сокета");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        handleError("Ошибка привязки сокета");
    }

    if (listen(server_fd, 3) < 0) {
        handleError("Ошибка начала прослушивания");
    }

    std::cout << "Сервер запущен на порту " << PORT << "..." << std::endl;

    while (true) {
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            std::cerr << "Ошибка подключения клиента" << std::endl;
            continue;
        }

        std::cout << "Клиент подключился" << std::endl;


        std::thread clientThread(handleClient, client_socket);
        clientThread.detach();
    }

    close(server_fd);
    return 0;
}

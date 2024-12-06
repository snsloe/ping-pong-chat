#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

#define PORT 8080
#define BUFFER_SIZE 1024

void handleError(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void saveHistory(const std::vector<std::string>& history) {
    FILE* file = fopen("chat_history.txt", "w");
    if (!file) {
        std::cerr << "Не удалось сохранить историю переписки." << std::endl;
        return;
    }

    for (const auto& line : history) {
        fprintf(file, "%s\n", line.c_str());
    }

    fclose(file);
    std::cout << "'chat_history.txt' сохранён.\n";
}

void clearLine() {
    std::cout << "\33[2K\r";
}

void receiveMessages(int sock, std::vector<std::string>& history, std::mutex& historyMutex) {
    char buffer[BUFFER_SIZE] = { 0 };

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE);

        if (valread > 0) {
            std::string server_message(buffer, valread);

            {
                std::lock_guard<std::mutex> lock(historyMutex);
                history.push_back("Server: " + server_message);
            }

            clearLine();
            std::cout << "Server: " << server_message << "\n";
            std::cout << "You: ";
            std::flush(std::cout);
        }
        else if (valread == 0) {
            clearLine();
            std::cout << "You  disconnected from the server.\n";
            break;
        }
        else {
            std::cerr << "Error reading from server.\n";
            break;
        }
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = { 0 };
    std::vector<std::string> history;
    std::mutex historyMutex;

    // Создание сокета
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handleError("Socket creation failed");
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        handleError("Invalid address or address not supported");
    }

    // Подключение к серверу
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        handleError("Connection to the server failed");
    }

    std::cout << "Connected to the server. Type 'exit' to quit.\n";

    std::thread receiveThread(receiveMessages, sock, std::ref(history), std::ref(historyMutex));

    while (true) {
        std::cout << "You: ";
        std::string client_message;
        std::getline(std::cin, client_message);

        {
            std::lock_guard<std::mutex> lock(historyMutex);
            history.push_back("You: " + client_message);
        }

        send(sock, client_message.c_str(), client_message.length(), 0);

        if (client_message == "exit") {
            break;
        }
    }

    if (receiveThread.joinable()) {
        receiveThread.join();
    }

    close(sock);

    saveHistory(history);

    return 0;
}

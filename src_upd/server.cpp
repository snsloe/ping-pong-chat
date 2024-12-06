#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <set>
#include <string>
#include <thread>
#include <mutex>


#define PORT 8080
#define BUFFER_SIZE 1024

std::vector<std::string> messageHistory;
std::mutex historyMutex;
std::set<int> clientSockets;
std::mutex clientSocketsMutex;

void handleError(const char* message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void broadcastMessage(const std::string& message, int sender_socket) {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);
    for (int client_socket : clientSockets) {
        if (client_socket != sender_socket) { 
            send(client_socket, message.c_str(), message.length(), 0);
        }
    }
}

void handleClient(int client_socket) {
    char buffer[BUFFER_SIZE] = { 0 };

    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        clientSockets.insert(client_socket);
    }

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(client_socket, buffer, BUFFER_SIZE);

        if (valread <= 0) {
            std::cerr << "Client disconnected or error occurred.\n";
            close(client_socket);
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets.erase(client_socket);
            }
            break;
        }

        std::string client_message(buffer, valread);
        client_message.erase(client_message.find_last_not_of("\r\n") + 1);

        if (client_message.empty()) {
            continue;
        }

        std::cout << "Received: " << client_message << std::endl;

        if (client_message == "ping") {
            const std::string pong_message = "Server: pong";
            send(client_socket, pong_message.c_str(), pong_message.length(), 0);
            {
                std::lock_guard<std::mutex> lock(historyMutex);
                messageHistory.push_back("Client: ping");
                messageHistory.push_back("Server: pong");
            }
        }
        else if (client_message == "history") {
            std::string history = "Message history:\n";
            {
                std::lock_guard<std::mutex> lock(historyMutex);
                for (const auto& entry : messageHistory) {
                    history += entry + "\n";
                }
            }
            send(client_socket, history.c_str(), history.length(), 0);
        }
        else if (client_message == "exit") {
            std::cout << "The client requested to disconnect.\n";
            close(client_socket);
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets.erase(client_socket);
            }
            break;
        }
        else {
            const std::string full_message = "Client: " + client_message;
            {
                std::lock_guard<std::mutex> lock(historyMutex);
                messageHistory.push_back(full_message);
            }
            broadcastMessage(full_message, client_socket);
        }
    }
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        handleError("Socket creation error.");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        handleError("Error setting socket options.");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Проверяем, можно ли забиндить порт
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Socket binding error. The server might already be running.");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        handleError("Error starting listening.");
    }

    std::cout << "Server is running on port " << PORT << "...\n";

    while (true) {
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            std::cerr << "Client connection error.\n";
            continue;
        }

        std::cout << "Client connected.\n";

        std::thread clientThread(handleClient, client_socket);
        clientThread.detach();
    }

    close(server_fd);
    return 0;
}

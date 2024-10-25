#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

const int BUFFER_SIZE = 1024;

class SocketClient {
private:
    static constexpr int PORT = 8080;
    SOCKET sock;

    void handleServerResponse(const std::string& command) {
        if (command == "list" || command == "getListApps") {
            // Just receive and display the response
            std::vector<char> buffer(BUFFER_SIZE);
            int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
            buffer[bytes_received] = '\0';
            std::cout << "Server response: " << std::string(buffer.data(), bytes_received) << std::endl;
        }
        else if (command == "screen capture" || command == "webcam capture") {
            receiveFile(command == "screen capture" ? "screenshot.bmp" : "webcam.jpg");
        }
        else if (command == "webcam record") {
            handleWebcamRecording();
        }
        else if (command == "runApp") {
            handleRunApp();
        }
        else if (command == "closeApp") {
            handleCloseApp();
        }
    }

    void receiveFile(const std::string& filename) {
        // Receive the file size using uint32_t instead of size_t
        uint32_t fileSize = 0;
        std::cout << "Waiting to receive file size..." << std::endl;

        int bytesReceived = recv(sock, reinterpret_cast<char*>(&fileSize), sizeof(uint32_t), 0);

        if (bytesReceived <= 0) {
            std::cerr << "Failed to receive the file size. Error code: " << WSAGetLastError() << std::endl;
            return;
        }

        std::cout << "File size received: " << fileSize << " bytes" << std::endl;

        // Prepare a buffer to receive the file data
        std::vector<char> buffer(fileSize);
        size_t totalBytesReceived = 0;

        std::cout << "Starting to receive file data..." << std::endl;

        // Loop to receive the file data in chunks until the entire file is received
        while (totalBytesReceived < fileSize) {
            size_t remainingBytes = fileSize - totalBytesReceived;
            size_t chunkSize = (remainingBytes < BUFFER_SIZE) ? remainingBytes : BUFFER_SIZE;

            int bytes = recv(sock, buffer.data() + totalBytesReceived,
                static_cast<int>(chunkSize), 0);
            if (bytes <= 0) {
                std::cerr << "Failed to receive file data. Error code: " << WSAGetLastError() << std::endl;
                return;
            }
            totalBytesReceived += bytes;
        }

        std::cout << "File received successfully." << std::endl;

        // Save the file to disk
        std::ofstream outFile(filename, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            //std::cerr << "Error: " << strerror(errno) << std::endl;
            return;
        }
        outFile.write(buffer.data(), fileSize);
        outFile.close();

        if (!outFile) {
            std::cerr << "Error occurred while writing file" << std::endl;
            return;
        }

        std::cout << "File saved as " << filename << std::endl;
    }

    void handleRunApp() {
        std::vector<char> buffer(BUFFER_SIZE);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received) << std::endl;
        int appIndex;
        std::cout << "Enter the index of the app you want to run: ";
        std::cin >> appIndex;
        send(sock, std::to_string(appIndex).c_str(), std::to_string(appIndex).size() + 1, 0);
    }

    void handleCloseApp() {
        std::vector<char> buffer(BUFFER_SIZE);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received) << std::endl;
        int appIndex;
        std::cout << "Enter the index of the app you want to close: ";
        std::cin >> appIndex;
        send(sock, std::to_string(appIndex).c_str(), std::to_string(appIndex).size() + 1, 0);
    }

    void handleWebcamRecording() {
        try {
            // Receive prompt from server
            std::vector<char> buffer(BUFFER_SIZE);
            int bytes_received = recv(sock, buffer.data(), BUFFER_SIZE - 1, 0);

            if (bytes_received <= 0) {
                throw std::runtime_error("Failed to receive server prompt for webcam recording");
            }

            buffer[bytes_received] = '\0';
            std::cout << "Server response: " << buffer.data();

            // Get recording duration from user
            std::string duration;
            std::getline(std::cin, duration);

            // Send duration to server
            int sent_bytes = send(sock, duration.c_str(), duration.length(), 0);
            if (sent_bytes <= 0) {
                throw std::runtime_error("Failed to send duration to server");
            }

            std::cout << "Waiting for server to start recording...\n";

            // Wait for server confirmation
            bytes_received = recv(sock, buffer.data(), BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                throw std::runtime_error("Failed to receive recording confirmation from server");
            }
            buffer[bytes_received] = '\0';

            // Check server response
            if (std::string(buffer.data()) == "RECORDING_STARTED") {
                std::cout << "Server has started recording. Please wait...\n";
                receiveFile("webcam_record.avi");
            }
            else {
                throw std::runtime_error("Unexpected server response: " + std::string(buffer.data()));
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error in handleWebcamRecording: " << e.what() << std::endl;
            const char* error_msg = "ERROR";
            send(sock, error_msg, strlen(error_msg), 0);
        }
    }

public:
    SocketClient() {
        initializeSocket();
    }

    void initializeSocket() {
        try {
            std::cout << "Initializing socket..." << std::endl;

            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
                throw std::runtime_error("WSAStartup failed: " + std::to_string(WSAGetLastError()));
            }

            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == INVALID_SOCKET) {
                throw std::runtime_error("Socket creation failed: " + std::to_string(WSAGetLastError()));
            }

            // Set keep-alive
            BOOL keepAlive = TRUE;
            if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive)) < 0) {
                std::cerr << "Warning: Failed to set keep-alive" << std::endl;
            }

            // Set receive timeout
            DWORD timeout = 30000; // 5 seconds
            if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
                std::cerr << "Warning: Failed to set receive timeout" << std::endl;
            }

            sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

            std::cout << "Attempting to connect to server..." << std::endl;
            if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
                throw std::runtime_error("Connection failed: " + std::to_string(WSAGetLastError()));
            }

            std::cout << "Successfully connected to server" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Socket initialization error: " << e.what() << std::endl;
            throw;
        }
    }

    void run() {
        std::string command;
        while (true) {
            std::cout << "\nEnter command: ";
            std::getline(std::cin, command);

            if (command == "exit") {
                break;
            }

            // Send command to server
            send(sock, command.c_str(), command.length(), 0);

            // Handle server response based on command
            handleServerResponse(command);
        }
    }

    ~SocketClient() {
        closesocket(sock);
        WSACleanup();
    }
};

int main() {
    try {
        SocketClient client;
        client.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
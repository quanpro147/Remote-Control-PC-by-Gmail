
#include <iostream>
#include <winsock2.h> // Thư viện Winsock
#include <ws2tcpip.h> 
#include <string>
#include <fstream>

#pragma comment(lib, "ws2_32.lib") // Liên kết thư viện Winsock
#define PORT 8080



int main() {
    // A. Đọc Email

    // B. Gửi lệnh tới server
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char buffer[1024] = { 0 };
    
    // 1. Khởi tạo Winsock
    std::cout << "Initializing Winsock..." << std::endl;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "Failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 2. Tạo socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cout << "Could not create socket: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 3. Cấu hình địa chỉ server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        std::cout << "Invalid address: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 4. Kết nối tới server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cout << "Connection failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 5. Gửi dữ liệu tới server
    std::string input_message;
    while (true) {
        std::cout << "Enter message to server (type 'list' to know detail): ";
        std::getline(std::cin, input_message);

        // Gửi tin nhắn từ người dùng đến server
        send(sock, input_message.c_str(), input_message.size() + 1, 0);

        // Kiểm tra nếu người dùng nhập 'exit' thì thoát chương trình
        if (input_message == "exit") {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        else if (input_message == "shutdown") {
            std::cout << "Shutdown command sent to server." << std::endl;
            break;
        }
    std::string makeGetRequest(const std::string& url) const {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        std::string response;

        else if (input_message == "list") {
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0'; // Đảm bảo chuỗi kết thúc bằng null
                std::cout << "Response from server: " << buffer << std::endl;
            }
        }

        // Nhận file screen capture từ server
        else if (input_message == "screen capture") {
            // Nhận kích thước file từ server
            size_t fileSize;
            int bytes_received = recv(sock, (char*)&fileSize, sizeof(fileSize), 0);
            if (bytes_received != sizeof(fileSize)) {
                std::cout << "Failed to receive file size." << std::endl;
                break;
            }

            // Nhận file ảnh từ server
            char* imgBuffer = new char[fileSize];
            size_t total_bytes_received = 0;
            bytes_received = recv(sock, imgBuffer, fileSize, 0);

            // Lưu file ảnh đã nhận
            std::ofstream outFile("received_screenshot.bmp", std::ios::binary);
            outFile.write(imgBuffer, fileSize);
            outFile.close();
            std::cout << "Screenshot saved as received_screenshot.bmp" << std::endl;

            delete[] imgBuffer;
        }

        // Nhận file ảnh từ webcam
        else if (input_message == "webcam capture") {
            // Nhận kích thước file từ server
            size_t fileSize;
            int bytes_received = recv(sock, (char*)&fileSize, sizeof(fileSize), 0);
            if (bytes_received == sizeof(fileSize)) {
                // Nhận file ảnh
                char* imgBuffer = new char[fileSize];
                bytes_received = recv(sock, imgBuffer, fileSize, 0);
                if (bytes_received == fileSize) {
                    // Lưu file ảnh đã nhận
                    std::ofstream outFile("received_webcam_image.jpg", std::ios::binary);
                    outFile.write(imgBuffer, fileSize);
                    outFile.close();
                    std::cout << "Webcam image saved as received_webcam_image.jpg" << std::endl;
                }
                else {
                    std::cout << "Failed to receive the complete webcam image." << std::endl;
                }

                delete[] imgBuffer;
            }

    //void handleServerCommand(const std::string& command) {
    //    std::vector<char> buffer(BUFFER_SIZE);
    //    std::string request;
    //    size_t colonPos = command.find(": ");
    //    if (colonPos != std::string::npos) {
    //        request = command.substr(colonPos + 2); // +2 to skip ": "
    //    }
    //    else {
    //        request = command; // If no ":" found, use the whole command
    //    }
    //    if (command == "exit" ) {
    //        throw std::runtime_error("Shutdown requested");
    //    }
    //    else if (request == "list" || request == "screen capture" ||
    //        request == "webcam capture" || request == "webcam record" || request == "shutdown") {
    //        send(sock, request.c_str(), request.size() + 1, 0);
    //        int bytes_received = recv(sock, buffer.data(), buffer.size(), 0);
    //        if (bytes_received > 0) {
    //            handleServerResponse(request, buffer.data(), bytes_received);
    //        }
    //    }
    //    else if (request == "no command"){
    //        
    //    }
    //    else {
    //        std::cout << "invalid command";
    //    }
    //}
    //void handleServerCommand(const std::string& command) {
    //    try {
    //        // Use a smaller, fixed buffer size
    //        constexpr size_t SAFE_BUFFER_SIZE = 1024;  // 1KB buffer
    //        std::vector<char> buffer(SAFE_BUFFER_SIZE);

    //        // If command is empty or too long, handle as invalid
    //        if (command.empty() || command.length() > SAFE_BUFFER_SIZE) {
    //            std::cout << "Invalid command length\n";
    //            return;
    //        }

    //        // Extract the request part after ": "
    //        std::string request = "invalid";
    //        size_t colonPos = command.find(": ");
    //        if (colonPos != std::string::npos && colonPos + 2 < command.length()) {
    //            request = command.substr(colonPos + 2); // +2 to skip ": "
    //        }
    //        else if (command == "invalid" || command == "no command") {
    //            request = command;
    //        }

    //        std::cout << "Parsed request: " << request << std::endl;  // Debug output

    //        // Handle the extracted request
    //        if (request == "exit") {
    //            throw std::runtime_error("Shutdown requested");
    //        }
    //        else if (request == "list" || request == "screen capture" ||
    //            request == "webcam capture" || request == "webcam record" ||
    //            request == "shutdown") {
    //            // Send only the request part to the server
    //            if (send(sock, request.c_str(), request.size(), 0) < 0) {
    //                std::cout << "Failed to send command to server\n";
    //                return;
    //            }

    //            int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
    //            if (bytes_received > 0) {
    //                buffer[bytes_received] = '\0';  // Ensure null termination
    //                handleServerResponse(request, buffer.data(), bytes_received);
    //            }
    //            else {
    //                std::cout << "No response from server\n";
    //            }
    //        }
    //        else if (request == "no command") {
    //            // Do nothing for "no command"
    //            return;
    //        }
    //        else if (request == "invalid") {
    //            // Do nothing for "invalid"
    //            return;
    //        }
    //        else {
    //            std::cout << "Invalid command: " << request << std::endl;
    //        }
    //    }
    //    catch (const std::bad_alloc& e) {
    //        std::cout << "Memory allocation error: " << e.what() << std::endl;
    //    }
    //    catch (const std::exception& e) {
    //        std::cout << "Error in handleServerCommand: " << e.what() << std::endl;
    //    }
    //}
    //void handleServerResponse(const std::string& command, const char* buffer, int bytes_received) {
    //    if (command == "list") {
    //        std::cout << "Server response: " << std::string(buffer, bytes_received) << std::endl;
    //    }
    //    else if (command == "screen capture" || command == "webcam capture") {
    //        receiveFile(command == "screen capture" ? "screenshot.bmp" : "webcam.jpg");
    //    }
    //    else if (command == "webcam record") {
    //        handleWebcamRecording();
    //    }
    //    else {

    //    }
    //}

    //void receiveFile(const std::string& filename) {
    //    size_t fileSize;
    //    if (recv(sock, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0) != sizeof(fileSize)) {
    //        throw std::runtime_error("Failed to receive file size");
    //    }

    //    std::vector<char> buffer(fileSize);
    //    if (recv(sock, buffer.data(), fileSize, 0) != fileSize) {
    //        throw std::runtime_error("Failed to receive file data");
    //    }

    //    std::ofstream outFile(filename, std::ios::binary);
    //    outFile.write(buffer.data(), fileSize);
    //    std::cout << "File saved as " << filename << std::endl;
    //}

    void handleWebcamRecording() {
        std::cout << "Enter recording duration (seconds): ";
        std::string duration;
        std::getline(std::cin, duration);
        send(sock, duration.c_str(), duration.size() + 1, 0);

        receiveFile("recording.avi");
            }
        }

        // Nhận video webcam từ server
        else if (input_message == "webcam record") {
            // Nhận phản hồi từ server về việc bắt đầu quay video
            int bytes_received_0 = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_received_0 > 0) {
                buffer[bytes_received_0] = '\0';
                std::cout << "Response from server: " << buffer << std::endl;

                // Nhập số giây muốn quay video
                std::string seconds;         
                std::getline(std::cin, seconds);

                // Gửi số giây đến server
                send(sock, seconds.c_str(), seconds.size() + 1, 0);
                // Nhận kích thước file video từ server
                size_t fileSize;
                int bytes_received = recv(sock, (char*)&fileSize, sizeof(fileSize), 0);
                if (bytes_received != sizeof(fileSize)) {
                    std::cout << "Failed to receive file size." << std::endl;
                }
                else {
                    // Nhận file video trong một lần duy nhất
                    char* videoBuffer = new char[fileSize];
                    bytes_received = recv(sock, videoBuffer, fileSize, 0);

                    if (bytes_received == fileSize) {
                        // Lưu video nhận được vào file
                        std::ofstream outFile("received_video.avi", std::ios::binary);
                        outFile.write(videoBuffer, fileSize);
                        outFile.close();
                        std::cout << "Video saved as received_video.avi" << std::endl;
                    }
                    else {
                        std::cout << "Failed to receive the video file." << std::endl;
                    }

                    delete[] videoBuffer;
                }             
                
            }

        }

        // Xóa buffer trước khi nhận phản hồi mới
        memset(buffer, 0, sizeof(buffer));
    }

    // 6. Đóng kết nối
    closesocket(sock);
    WSACleanup();

    return 0;
}

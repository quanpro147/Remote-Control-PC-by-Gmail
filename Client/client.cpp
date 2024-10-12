#include <iostream>
#include <winsock2.h> // Thư viện Winsock
#include <ws2tcpip.h> 
#include <string>
#include <fstream>
#pragma comment(lib, "ws2_32.lib") // Liên kết thư viện Winsock

#define PORT 8080

int main() {
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
            else {
                std::cout << "Failed to receive file size." << std::endl;
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
                std::cout << "Enter the number of seconds to record: ";
                std::getline(std::cin, seconds);

                // Gửi số giây đến server
                send(sock, seconds.c_str(), seconds.size() + 1, 0);
                // Nhận kích thước file video từ server
                size_t fileSize;
                int video_length = recv(sock, (char*)&fileSize, sizeof(fileSize), 0);
                if (video_length == sizeof(fileSize)) {

                    // Nhận file video
                    char* videoBuffer = new char[fileSize];
                    size_t total_bytes_received = 0;

                    while (total_bytes_received < fileSize) {
                        video_length = recv(sock, videoBuffer + total_bytes_received, fileSize - total_bytes_received, 0);
                        if (video_length <= 0) {
                            std::cout << "Failed to receive complete video file." << std::endl;
                            break;
                        }
                        total_bytes_received += video_length;
                    }

                    if (total_bytes_received == fileSize) {
                        // Lưu video nhận được vào file
                        std::ofstream outFile("received_video.avi", std::ios::binary);
                        outFile.write(videoBuffer, fileSize);
                        outFile.close();
                        std::cout << "Video saved as received_video.avi" << std::endl;
                    }

                    delete[] videoBuffer;
                }
                else {
                    std::cout << "Failed to receive file size." << std::endl;
                }
            }

            // Xóa buffer trước khi nhận phản hồi mới
            memset(buffer, 0, sizeof(buffer));
        }
    }

    // 6. Đóng kết nối
    closesocket(sock);
    WSACleanup();

    return 0;
}

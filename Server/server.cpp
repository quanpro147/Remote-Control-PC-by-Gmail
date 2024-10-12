#include <iostream>
#include <winsock2.h> // Winsock library
#include <fstream>    // Thư viện xử lý file
#include <ctime>      // Thư viện xử lý thời gian
#include <stdio.h>
#include <windows.h>
#include <opencv2/opencv.hpp> // Thư viện OpenCV
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp> // Để lưu ảnh

//#include <opencv2/opencv.hpp>
#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define PORT 8080
// Các hàm chức năng

// Hàm Screen capture
void SaveBitmapToFile(HBITMAP hBitmap, const std::string& file_path) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;
    DWORD dwSize;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24; // 24-bit bitmap
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    dwSize = ((bmp.bmWidth * 3 + 3) & ~3) * bmp.bmHeight;

    // Tạo file để lưu ảnh
    HANDLE hFile = CreateFileA(file_path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    bmfHeader.bfType = 0x4D42; // 'BM'
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwSize;
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    DWORD dwWritten;
    WriteFile(hFile, &bmfHeader, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
    WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwWritten, NULL);

    BYTE* pBitmap = new BYTE[dwSize];
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    GetDIBits(hMemoryDC, hBitmap, 0, bmp.bmHeight, pBitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    WriteFile(hFile, pBitmap, dwSize, &dwWritten, NULL);

    // Dọn dẹp
    delete[] pBitmap;
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    CloseHandle(hFile);
}
void TakeScreenshot(const std::string& file_path) {
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    // Lưu bitmap vào file
    SaveBitmapToFile(hBitmap, file_path);

    // Dọn dẹp
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}


// Hàm tắt máy
void ShutdownSystem() {
    // EWX_SHUTDOWN: Tắt máy
    // EWX_FORCE: Buộc các ứng dụng đóng mà không cảnh báo người dùng
    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER)) {
        std::cout << "Shutdown failed. Error: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "System is shutting down..." << std::endl;
    }
}


// Hàm webcam capture
void CaptureWebcamImage(const std::string& file_path) {
    // Mở webcam
    cv::VideoCapture cap(0); // 0 là ID của webcam mặc định
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open webcam." << std::endl;
        return;
    }

    // Đọc frame từ webcam
    cv::Mat frame;
    cap >> frame;

    // Kiểm tra xem frame có rỗng không
    if (frame.empty()) {
        std::cerr << "Error: Could not capture image from webcam." << std::endl;
        return;
    }

    // Lưu ảnh vào file
    cv::imwrite(file_path, frame);

    // Đóng webcam
    cap.release();
}


// Hàm quay video từ webcam
bool RecordVideoFromWebcam(const std::string& output_file, int duration_in_seconds) {
    // Mở webcam (0 là chỉ số của webcam mặc định)
    cv::VideoCapture cap(0);

    // Kiểm tra nếu không mở được webcam
    if (!cap.isOpened()) {
        std::cout << "Cannot open the webcam" << std::endl;
        return false;
    }

    // Lấy kích thước và FPS của video từ webcam
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

    // Định nghĩa codec và tạo đối tượng VideoWriter để lưu video
    cv::VideoWriter video(output_file,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        fps,
        cv::Size(frame_width, frame_height));

    if (!video.isOpened()) {
        std::cout << "Error: Cannot open video file to save" << std::endl;
        return false;
    }

    cv::Mat frame;
    int total_frames = fps * duration_in_seconds; // Tổng số frame sẽ ghi dựa trên FPS và thời gian

    std::cout << "Recording video for " << duration_in_seconds << " seconds..." << std::endl;

    for (int i = 0; i < total_frames; ++i) {
        cap >> frame;  // Lấy một frame từ webcam

        if (frame.empty()) {
            std::cout << "Failed to capture image" << std::endl;
            break;
        }

        // Ghi frame vào video
        video.write(frame);

        // Hiển thị frame cho mục đích xem trực tiếp
        cv::imshow("Webcam", frame);

        // Thoát nếu nhấn phím 'q' trong khi quay video
        if (cv::waitKey(30) == 'q') {
            std::cout << "Stopping video recording." << std::endl;
            break;
        }
    }

    // Giải phóng tài nguyên
    cap.release();
    video.release();
    cv::destroyAllWindows();

    return true;
}



int main() {
    WSADATA wsa;
    SOCKET server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[1024] = { 0 };
    const char* response = "Command not recognized.";

    // 1. Khởi tạo Winsock
    std::cout << "Initializing Winsock..." << std::endl;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "Failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 2. Tạo socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cout << "Could not create socket: " << WSAGetLastError() << std::endl;
        return 1;
    }
    std::cout << "Socket created." << std::endl;

    // 3. Cấu hình địa chỉ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 4. Gán địa chỉ cho socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        return 1;
    }
    std::cout << "Bind done." << std::endl;

    // 5. Lắng nghe kết nối
    listen(server_socket, 3);
    std::cout << "Waiting for incoming connections..." << std::endl;

    // 6. Chấp nhận kết nối
    if ((new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len)) == INVALID_SOCKET) {
        std::cout << "Accept failed: " << WSAGetLastError() << std::endl;
        return 1;
    }
    std::cout << "Connection accepted." << std::endl;

    // 7. Xử lý lệnh từ client
    while (true) {
        memset(buffer, 0, sizeof(buffer)); // Xóa nội dung buffer trước khi nhận dữ liệu mới
        int recv_size = recv(new_socket, buffer, 1024, 0);
        if (recv_size == SOCKET_ERROR) {
            std::cout << "Recv failed: " << WSAGetLastError() << std::endl;
            break;
        }
        else if (recv_size == 0) {
            std::cout << "Client disconnected." << std::endl;
            break; // Kết thúc khi client ngắt kết nối
        }
        buffer[recv_size] = '\0'; // Đảm bảo chuỗi kết thúc bằng null
       
        // Xử lý lệnh từ client
        std::cout << "Message from client: " << buffer << std::endl;

		// Gửi danh sách các lệnh có thể thực thi
        if (strcmp(buffer, "list") == 0) {
            const char* available_commands =
                "Available commands:\n"
                "1. Shutdown PC\n"
                "2. Restart PC\n"
                "3. Log out\n"
                "4. Log in\n"
                "5. Screen capture\n"
                "6. Webcam capture\n"
                "7. Webcam record\n";
            send(new_socket, available_commands, strlen(available_commands), 0);
            std::cout << "Sent list of available commands to client." << std::endl;
        }

        // Xử lý lệnh exit
        else if (strcmp(buffer, "exit") == 0) {
            std::cout << "Client sent exit command. Closing connection..." << std::endl;
            break; // Thoát vòng lặp khi client gửi lệnh "exit"
        }

        // Xử lý lệnh shutdown
        else if (strcmp(buffer, "shutdown") == 0) {
            std::cout << "Shutdown command received, shutting down..." << std::endl;
            ShutdownSystem();
        }

		// Xử lý lệnh screen capture
        else if (strcmp(buffer, "screen capture") == 0) {
            // Chụp ảnh từ màn hình và lưu vào file
            TakeScreenshot("D:\\Hp\\Pictures\\Screenshots\\screenshot.bmp");

            // Mở file ảnh để gửi
            std::ifstream imgFile("D:\\Hp\\Pictures\\Screenshots\\screenshot.bmp", std::ios::binary);
            if (imgFile) {
                imgFile.seekg(0, std::ios::end);
                size_t fileSize = imgFile.tellg(); // Lấy kích thước file
                imgFile.seekg(0, std::ios::beg);

                // Gửi kích thước file trước
                send(new_socket, (char*)&fileSize, sizeof(fileSize), 0);

                // Gửi dữ liệu file ảnh
                char* imgBuffer = new char[fileSize];
                imgFile.read(imgBuffer, fileSize);
                send(new_socket, imgBuffer, fileSize, 0);

                // Dọn dẹp
                delete[] imgBuffer;
                imgFile.close();
                std::cout << "Sent screenshot to client." << std::endl;         
            }
            else {
                std::cout << "Failed to open screenshot file." << std::endl;
            }
        }

        // Xử lý lệnh webcam capture
		else if (strcmp(buffer, "webcam capture") == 0) {

			// Chụp ảnh từ webcam và lưu vào file
			CaptureWebcamImage("D:\\Hp\\Pictures\\Screenshots\\webcam_image.jpg");

			// Mở file ảnh để gửi
			std::ifstream imgFile("D:\\Hp\\Pictures\\Screenshots\\webcam_image.jpg", std::ios::binary);
			if (imgFile) {
				imgFile.seekg(0, std::ios::end);
				size_t fileSize = imgFile.tellg(); // Lấy kích thước file
				imgFile.seekg(0, std::ios::beg);

				// Gửi kích thước file trước
				send(new_socket, (char*)&fileSize, sizeof(fileSize), 0);

				// Gửi dữ liệu file ảnh
				char* imgBuffer = new char[fileSize];
				imgFile.read(imgBuffer, fileSize);
				send(new_socket, imgBuffer, fileSize, 0);

				// Dọn dẹp
				delete[] imgBuffer;
				imgFile.close();                
				std::cout << "Sent webcam image to client." << std::endl;
			}
			else {
				std::cout << "Failed to open webcam image file." << std::endl;
			}
		}

		// Xử lý lệnh webcam record
        else if (strcmp(buffer, "webcam record") == 0) {
            // Gửi yêu cầu nhập số giây muốn quay video
            std::string prompt = "Enter the number of seconds to record: ";
            send(new_socket, prompt.c_str(), prompt.size() + 1, 0);

            // Nhận số giây từ client
            char timeBuffer[10];
            int time_received = recv(new_socket, timeBuffer, sizeof(timeBuffer), 0);
            if (time_received <= 0) {
                std::cout << "Failed to receive duration from client." << std::endl;
                break;
            }
            timeBuffer[time_received] = '\0';
            int duration = std::stoi(timeBuffer);

            // Quay video và lưu thành file
            RecordVideoFromWebcam("D:\\Hp\\Videos\\output_video.avi", duration);

            // Gửi video cho client
            std::ifstream videoFile("D:\\Hp\\Videos\\output_video.avi", std::ios::binary);
            if (videoFile) {
                // 1. Lấy kích thước file
                videoFile.seekg(0, std::ios::end);
                size_t fileSize = videoFile.tellg();
                videoFile.seekg(0, std::ios::beg);

                // 2. Gửi kích thước file cho client
                send(new_socket, (char*)&fileSize, sizeof(fileSize), 0);

                // 3. Đọc dữ liệu từ file vào buffer và gửi đi
                char* videoBuffer = new char[fileSize];
                videoFile.read(videoBuffer, fileSize);
                send(new_socket, videoBuffer, fileSize, 0);

                // 4. Giải phóng bộ nhớ và đóng file
                delete[] videoBuffer;
                videoFile.close();
                std::cout << "Sent video to client." << std::endl;
            }
        }


		// Xử lý các lệnh không hợp lệnh
        else {
            send(new_socket, response, strlen(response), 0);
            std::cout << "Response sent to client." << std::endl;
        }
    }

    // 8. Đóng kết nối
    closesocket(new_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}


#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <iostream>
#include <winsock2.h> 
#include <fstream>    
#include <ctime>      
#include <stdio.h>
#include <windows.h>
#include <opencv2/opencv.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp> 
#include <psapi.h>
#include <locale>
#include <codecvt>
#include <tlhelp32.h>
#include <psapi.h>
#include <gdiplus.h>
#include <cstdio>
#include <winsvc.h>
#include<string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Advapi32.lib")

#define PORT 8080
const int BUFFER_SIZE = 1024;

// Các hàm phụ
void sendData(SOCKET new_socket, const std::string& data) {
    int totalSent = 0;
    int dataLength = data.length();
    while (totalSent < dataLength) {
        int sent = send(new_socket, data.c_str() + totalSent, dataLength - totalSent, 0);
        if (sent == SOCKET_ERROR) {
            std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
            return;
        }
        totalSent += sent;
    }
}
void handleSendFile(const std::string& file_path, SOCKET new_socket) {
    std::ifstream file(file_path, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);      
        uint32_t fileSize = static_cast<uint32_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        if (send(new_socket, reinterpret_cast<const char*>(&fileSize), sizeof(uint32_t), 0) < 0) {
            std::cerr << "Failed to send file size. Error: " << WSAGetLastError() << std::endl;
            file.close();
            return;
        }

        // Rest of the code remains the same
        char* buffer = new char[fileSize];
        file.read(buffer, fileSize);

        size_t totalBytesSent = 0;
        while (totalBytesSent < fileSize) {
            int result = send(new_socket, buffer + totalBytesSent, fileSize - totalBytesSent, 0);
            if (result < 0) {
                std::cerr << "Failed to send file data." << std::endl;
                delete[] buffer;
                file.close();
                return;
            }
            totalBytesSent += result;
        }

        delete[] buffer;
        file.close();
        std::cout << "Sent file to client." << std::endl;
    }
    else {
        std::cerr << "Failed to open file." << std::endl;
    }
}
// Hàm xóa file
bool handleDeleteFile(const std::string& fileName) {
    std::string filePath = "./list file/" + fileName;
    // Thực hiện xóa file
    if (std::filesystem::remove(filePath)) {
        return true;
    }
    else {
        std::cout << "System is shutting down..." << std::endl;
    }
}

class GdiplusInitializer {
public:
    GdiplusInitializer() {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    }
    ~GdiplusInitializer() {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }
private:
    ULONG_PTR gdiplusToken;
};
// Hàm chụp ảnh
void SaveScreenshotToJPG(const std::wstring& file_path, ULONG quality = 90) {
    // Khởi tạo GDI+
    GdiplusInitializer gdiplusInit;

    // Lấy kích thước màn hình
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // Tạo DC và bitmap
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    // Chụp màn hình
    BitBlt(memDC, 0, 0, width, height, screenDC, 0, 0, SRCCOPY);

    // Chuyển HBITMAP sang Gdiplus::Bitmap
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hBitmap, NULL);

    // Cấu hình encoder cho JPEG
    CLSID encoderClsid;
    {
        const WCHAR* format = L"image/jpeg";
        UINT num = 0;
        UINT size = 0;
        Gdiplus::GetImageEncodersSize(&num, &size);
        if (size > 0) {
            std::unique_ptr<Gdiplus::ImageCodecInfo[]> pImageCodecInfo(new Gdiplus::ImageCodecInfo[size]);
            Gdiplus::GetImageEncoders(num, size, pImageCodecInfo.get());
            for (UINT j = 0; j < num; ++j) {
                if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
                    encoderClsid = pImageCodecInfo[j].Clsid;
                    break;
                }
            }
        }
    }

    // Thiết lập chất lượng JPEG
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    // Lưu file
    bitmap->Save(file_path.c_str(), &encoderClsid, &encoderParams);

    // Dọn dẹp
    delete bitmap;
    SelectObject(memDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);
}
void TakeScreenshot(const std::string& file_path, ULONG quality = 90) {
    // Chuyển đổi string sang wstring
    std::wstring wide_path(file_path.begin(), file_path.end());
    SaveScreenshotToJPG(wide_path, quality);
}
void CaptureWebcamImage(const std::string& file_path) {
   
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open webcam." << std::endl;
        return;
    }
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "Error: Could not capture image from webcam." << std::endl;
        return;
    }
    cv::imwrite(file_path, frame);
    cap.release();
}
bool RecordVideoFromWebcam(const std::string& output_file, int duration_in_seconds) {
    cv::VideoCapture cap(0);

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
    cap.release();
    video.release();
    cv::destroyAllWindows();
    return true;
}
// Hàm lấy danh sách dịch vụ
std::vector<std::wstring> getServicesList() {
    std::vector<std::wstring> services_list;
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);

    if (scManager == NULL) {
        std::wcerr << L"Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return services_list;
    }

    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;

    // First call to get required buffer size
    EnumServicesStatusEx(
        scManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        NULL,
        0,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL
    );

    if (bytesNeeded == 0) {
        CloseServiceHandle(scManager);
        return services_list;
    }

    // Allocate memory for services
    LPBYTE lpServices = new BYTE[bytesNeeded];
    ENUM_SERVICE_STATUS_PROCESS* services = (ENUM_SERVICE_STATUS_PROCESS*)lpServices;

    // Second call to get actual data
    if (EnumServicesStatusEx(
        scManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        lpServices,
        bytesNeeded,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL
    )) {
        // Process each service
        for (DWORD i = 0; i < servicesReturned; i++) {
            std::wstring serviceName = services[i].lpServiceName;
            std::wstring displayName = services[i].lpDisplayName;
            std::wstring serviceStatus;

            // Convert service status to string
            switch (services[i].ServiceStatusProcess.dwCurrentState) {
            case SERVICE_RUNNING:
                serviceStatus = L"Running";
                break;
            case SERVICE_STOPPED:
                serviceStatus = L"Stopped";
                break;
            case SERVICE_PAUSED:
                serviceStatus = L"Paused";
                break;
            case SERVICE_START_PENDING:
                serviceStatus = L"Starting";
                break;
            case SERVICE_STOP_PENDING:
                serviceStatus = L"Stopping";
                break;
            case SERVICE_PAUSE_PENDING:
                serviceStatus = L"Pausing";
                break;
            case SERVICE_CONTINUE_PENDING:
                serviceStatus = L"Continuing";
                break;
            default:
                serviceStatus = L"Unknown";
            }

            // Format service information
            std::wstring serviceInfo = std::to_wstring(i + 1) + L". " +
                displayName + L" (" + serviceName + L") - " +
                serviceStatus;
            services_list.push_back(serviceInfo);
        }
    }

    delete[] lpServices;
    CloseServiceHandle(scManager);
    return services_list;
}
void handleGetServices(SOCKET new_socket) {
    std::vector<std::wstring> services_list = getServicesList();

    // Convert the list to string for sending
    std::string services_str = "Windows Services:\n";
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    for (const std::wstring& service : services_list) {
        services_str += converter.to_bytes(service) + "\n";
    }

    // Send the services list to client
    sendData(new_socket, services_str);
}

std::vector<std::wstring> getInstalledApps() {
    std::vector<std::wstring> app_list = { L"chrome",L"Word",L"Excel",L"Edge",L"Paint",L"Explorer",L"Notepad",L"Calculator",L"snipping tool",L"Power Point",L"camera" };
    

    return app_list;
}
std::vector<LPCWSTR>getCommand() {
    std::vector<LPCWSTR>app_Command = { L"chrome.exe",L"winword.exe",L"excel.exe",L"msedge.exe",L"mspaint.exe",L"explorer.exe",L"notepad.exe",L"calc.exe",L"snippingtool.exe",L"powerpnt.exe" ,L"microsoft.windows.camera"};
    return app_Command;
}
// Hàm chạy ứng dụng
bool runApp(const std::vector<std::wstring>& app_list, int appIndex) {
    std::vector<LPCWSTR>AppCommand = getCommand();
    if (appIndex >= 0 && appIndex < app_list.size()) {
        HINSTANCE result = ShellExecute(
            NULL,          // Handle tới cửa sổ (NULL nếu không cần)
            L"open",       // Hành động (open, print, explore, ...)
            AppCommand[appIndex], // Đường dẫn tới ứng dụng
            NULL, // Tham số truyền vào ứng dụng (nếu có)
            NULL,          // Thư mục làm việc (NULL = mặc định)
            SW_SHOWNORMAL  // Hiển thị cửa sổ ứng dụng (SW_HIDE, SW_MINIMIZE, ...)
        );
        if ((int)result <= 32) {
            std::wcerr << L"cannot start the application" << L"\n";
            return false;
        }
        else {
            std::wcout << L"successfully  start the application:  " << L"\n";
            return true;
        }
       
}
    return false;
    
}

bool CloseApplication(const std::wstring& executablePath) {
    // Extract executable name from path
    size_t lastBackslash = executablePath.find_last_of(L"\\");
    std::wstring executableName = (lastBackslash != std::wstring::npos) ?
        executablePath.substr(lastBackslash + 1) : executablePath;

    bool processFound = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::wcout << L"Failed to create process snapshot." << std::endl;
        return false;
    }

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(processEntry);

    // Iterate through all processes
    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            if (_wcsicmp(processEntry.szExeFile, executableName.c_str()) == 0) {
                HANDLE processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, processEntry.th32ProcessID);
                if (processHandle != NULL) {
                    if (TerminateProcess(processHandle, 0)) {
                        processFound = true;
                        std::wcout << L"Successfully terminated process: " << executableName << std::endl;
                    }
                    else {
                        std::wcout << L"Failed to terminate process: " << executableName <<
                            L" (Error: " << GetLastError() << L")" << std::endl;
                    }
                    CloseHandle(processHandle);
                }
            }
        } while (Process32NextW(snapshot, &processEntry));
    }

    CloseHandle(snapshot);

    if (!processFound) {
        std::wcout << L"No running process found for: " << executableName << std::endl;
        return false;
    }

    return true;
}
// Function to handle the closeApp command
void handleCloseApp(const std::vector<std::wstring>& app_list, SOCKET new_socket) {
    // Send list of running applications to client
    std::string app_list_str = "Choose an app to close:\n";
    for (int i = 0; i < app_list.size(); ++i) {
        app_list_str += std::to_string(i) + ". " +
            std::string(app_list[i].begin(), app_list[i].end()) + "\n";
    }
    send(new_socket, app_list_str.c_str(), app_list_str.size() + 1, 0);

    // Receive app index from client
    char appIndexBuffer[10];
    int appIndexReceived = recv(new_socket, appIndexBuffer, sizeof(appIndexBuffer), 0);
    if (appIndexReceived <= 0) {
        std::cout << "Failed to receive app index from client." << std::endl;
        return;
    }
    appIndexBuffer[appIndexReceived] = '\0';
    int appIndex = std::stoi(appIndexBuffer);

    // Validate index
    if (appIndex < 0 || appIndex >= app_list.size()) {
        std::string error_msg = "Invalid app index.";
        send(new_socket, error_msg.c_str(), error_msg.size() + 1, 0);
        return;
    }

    // Extract executable path
    const std::wstring& appInfo = app_list[appIndex];
    size_t separatorPos = appInfo.find(L" - ");
    if (separatorPos == std::wstring::npos) {
        std::string error_msg = "Invalid application information.";
        send(new_socket, error_msg.c_str(), error_msg.size() + 1, 0);
        return;
    }

    std::wstring appPath = appInfo.substr(separatorPos + 3);
    // Remove quotes if present
    if (appPath.front() == L'"' && appPath.back() == L'"') {
        appPath = appPath.substr(1, appPath.length() - 2);
    }

    // Try to close the application
    bool success = CloseApplication(appPath);

    // Send result to client
    std::string result_msg = success ? "Application closed successfully." :
        "Failed to close application.";
    send(new_socket, result_msg.c_str(), result_msg.size() + 1, 0);
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
        buffer[recv_size] = '\0';
       
        // Xử lý lệnh từ client
        std::cout << "Message from client: " << buffer << std::endl;
        std::string message(buffer);
        size_t delimiterPos = message.find(": ");
        std::string sender = "";
        std::string request = "";
        if (delimiterPos != std::string::npos) {
            sender = message.substr(0, delimiterPos); // tách sender
            request = message.substr(delimiterPos + 2); // tách request
        }
        else {
            std::cout << "Invalid command!" << std::endl;
            continue;
        }
        
		// Gửi danh sách các lệnh có thể thực thi
        if (request == "list") {
            const char* available_commands =
                "Available commands:\n"
                "1. Shutdown PC\n"
                "2. Restart PC\n"
                "3. Log in pc\n"
                "4. Log out pc\n"
                "5. Screen capture\n"
                "6. Webcam capture\n"
                "7. Webcam record\n"
                "8. Get file\n"
                "9. Get list apps\n"
                "10. Run app\n"
                "11. Close app\n"
                "12. List services\n";
            send(new_socket, available_commands, strlen(available_commands), 0);
            std::cout << "Sent list of available commands to client." << std::endl;
        }


		else if (request == "list services") {
            handleGetServices(new_socket);
            std::cout << "Sent list of services to client." << std::endl;			
		}
        
        else if (request == "stop service") {
            //handleStopService();
        }

        else if (request == "exit") {
            std::cout << "Client sent exit command. Closing connection..." << std::endl;
            break;
        }

     
        else if (request == "shutdown") {
            std::cout << "Shutdown command received, shutting down..." << std::endl;
            ShutdownSystem();
        }

   
        else if (request == "log in") {}


		else if (request == "log out") {}

	
        else if (request == "screen capture") {
            TakeScreenshot("screenshot.bmp");       
			handleSendFile("screenshot.bmp", new_socket);
        }

		else if (request == "webcam capture") {
			CaptureWebcamImage("webcam_image.jpg");
			handleSendFile("webcam_image.jpg", new_socket);
		}

        else if (request == "webcam record") {
   
            std::string prompt = "Enter the number of seconds to record: ";
            send(new_socket, prompt.c_str(), prompt.size() + 1, 0);
            
       
            std::cout << "Waiting duration from client...\n";
            char timeBuffer[10];
            int time_received = recv(new_socket, timeBuffer, sizeof(timeBuffer), 0);
            if (time_received <= 0) {
                std::cout << "Failed to receive duration from client." << std::endl;
                break;
            }
            timeBuffer[time_received] = '\0';
            int duration = std::stoi(timeBuffer);

            RecordVideoFromWebcam("webcam_video.avi", duration);
			std::string response = "ok";
            send(new_socket, "ok", response.size() + 1, 0);
            handleSendFile("webcam_video.avi", new_socket);
        }

        else if (request == "getFile") {
            std::string prompt = "Enter file name: ";
            send(new_socket, prompt.c_str(), prompt.size() + 1, 0);
            recv(new_socket, buffer, BUFFER_SIZE, 0);
            std::string filepath(buffer);
            std::cout << "Request file from client: " << filepath << std::endl;
            handleSendFile(filepath, new_socket);       
        }

        else if (request == "getListApps") {    
            std::vector<std::wstring> app_list = getInstalledApps();
            std::string app_list_str = "Installed applications:\n";
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            for (const std::wstring& app : app_list) {
                std::string app_str = converter.to_bytes(app);
                app_list_str += app_str + "\n";
            }
            size_t total_bytes_sent = 0;
            size_t bytes_to_send = app_list_str.size() + 1;
            const char* data = app_list_str.c_str();

            while (total_bytes_sent < bytes_to_send) {
                int bytes_sent = send(new_socket, data + total_bytes_sent, bytes_to_send - total_bytes_sent, 0);
                if (bytes_sent == SOCKET_ERROR) {
                    std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
                    break;
                }
                total_bytes_sent += bytes_sent;
            }
			std::cout << "Sent list of installed apps to client." << std::endl;
        }

        else if (request == "runApp") {
            std::vector<std::wstring> app_list = getInstalledApps();


			std::string app_list_str = "Choose an app to run:\n";
			for (int i = 0; i < app_list.size(); ++i) {
				app_list_str += std::to_string(i)+ std::string(app_list[i].begin(), app_list[i].end()) + "\n";              
			}
			send(new_socket, app_list_str.c_str(), app_list_str.size() + 1, 0);


            std::cout << "Waiting app index from client...\n";
			char appIndexBuffer[10];
			int appIndexReceived = recv(new_socket, appIndexBuffer, sizeof(appIndexBuffer), 0);
			if (appIndexReceived <= 0) {
				std::cout << "Failed to receive app index from client." << std::endl;
				break;
			}
			appIndexBuffer[appIndexReceived] = '\0';
			int appIndex = std::stoi(appIndexBuffer);


			bool Check = runApp(app_list, appIndex);
            if (Check) {
                const char* MESS = "successfully start application ";
                send(new_socket, MESS, strlen(MESS), 0);
            }
            else {
                const char* MESS = "cannot start application ";
                send(new_socket, MESS, strlen(MESS), 0);
            }
        }

        else if (request == "closeApp") {
            std::vector<std::wstring> app_list = getInstalledApps();
            handleCloseApp(app_list, new_socket);
        }

        else {
            send(new_socket, response, strlen(response), 0);
            std::cout << "No valid request" << std::endl;
        }
    }


    closesocket(new_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}


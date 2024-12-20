#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h> 
#include <fstream>    
#include <ctime>      
#include <stdio.h>
#include <windows.h>
#include <opencv2/opencv.hpp> 
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/utils/logger.hpp>
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
#include <filesystem>
#include "json.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Advapi32.lib")
#define PORT 8080
const int BUFFER_SIZE = 1024;
using json = nlohmann::json;

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
// Các hàm chức năng
// Hàm tắt máy
void ShutdownSystem() {
    if (!ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER)) {
        std::cout << "Shutdown failed. Error: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "System is shutting down..." << std::endl;
    }
}

// Hàm lấy danh sách file
std::vector<std::string> getFileList(const std::string& directory = "./list file") {
    std::vector<std::string> fileList;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                fileList.push_back(entry.path().filename().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing directory: " << e.what() << std::endl;
    }
    return fileList;
}
// Hàm lấy file
void handleSendFile(const std::string& filePath, SOCKET new_socket) {
    std::ifstream file(filePath, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        uint32_t fileSize = static_cast<uint32_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        if (send(new_socket, reinterpret_cast<const char*>(&fileSize), sizeof(uint32_t), 0) < 0) {
            std::cerr << "Failed to send file size. Error: " << WSAGetLastError() << std::endl;
            file.close();
            return;
        }

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
        std::cerr << "File not found or could not be deleted: " << filePath << std::endl;
        return false;
    }
}

// Hàm chụp ảnh
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

 //Hàm quay video webcam
bool recordVideo(const std::string& output_file, int duration_in_seconds) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
    cv::VideoCapture cap(0);

    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fps = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
    cv::VideoWriter video(output_file,
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
        fps,
        cv::Size(frame_width, frame_height));

    if (!video.isOpened()) {
        std::cout << "Error: Cannot open video file to save" << std::endl;
        return false;
    }

    cv::Mat frame;
    int total_frames = fps * duration_in_seconds;

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

// Hàm lấy danh sách ứng dụng
std::vector<std::wstring> getListApps() {
    std::vector<std::wstring> app_list = { L"1. Chrome",L"2. Word",L"3. Excel",L"4. Edge",L"5. Paint",L"6. Explorer",L"7. Notepad",
                                            L"8. Calculator",L"9. Snipping tool",L"10. Power Point",L"11. Camera" };
    return app_list;
}
std::vector<LPCWSTR>getCommand() {
    std::vector<LPCWSTR>app_Command = { L"chrome.exe",L"winword.exe",L"excel.exe",L"msedge.exe",L"mspaint.exe",L"explorer.exe",
                                            L"notepad.exe",L"calc.exe",L"snippingtool.exe",L"powerpnt.exe" ,L"microsoft.windows.camera"};
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

// Hàm dừng ứng dụng
bool stopApp(const std::wstring& appName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot.\n";
        return false;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    bool isStopped = false;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            // So sánh tên tiến trình
            if (_wcsicmp(pe32.szExeFile, appName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 0)) {
                        std::wcout << L"Successfully terminated: " << appName << L"\n";
                        isStopped = true;
                    }
                    else {
                        std::wcerr << L"Failed to terminate: " << appName << L"\n";
                    }
                    CloseHandle(hProcess);
                }
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return isStopped;
}
std::vector<std::wstring> getRunningApps(const std::vector<LPCWSTR>& appCommands) {
    std::set<std::wstring> uniqueApps;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot.\n";
        return {};
    }
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            for (const auto& command : appCommands) {
                if (_wcsicmp(pe32.szExeFile, command) == 0) {
                    uniqueApps.insert(pe32.szExeFile); 
                    break;
                }
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return std::vector<std::wstring>(uniqueApps.begin(), uniqueApps.end());
}
void handleStopApp(SOCKET new_socket) {
    std::vector<LPCWSTR> appCommands = getCommand();
    std::vector<std::wstring> runningFilteredApps = getRunningApps(appCommands);
    std::wstring response = L"List of running applications:\n";
    for (size_t i = 0; i < runningFilteredApps.size(); ++i) {
        response += std::to_wstring(i+1) + L". " + runningFilteredApps[i] + L"\n";
    }
    response += L"Choose app to stop:";
    send(new_socket, (char*)response.c_str(), response.size() * sizeof(wchar_t), 0);
    char buffer[10];
    int received = recv(new_socket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        std::cerr << "Failed to receive index from client.\n";
        return;
    }
    buffer[received] = '\0';
    int appIndex = std::stoi(buffer);
    if (appIndex < 0 || appIndex >= runningFilteredApps.size()) {
        std::cerr << "Invalid app index received.\n";
        return;
    }
    if (stopApp(runningFilteredApps[appIndex-1])) {
        std::wstring successMsg = L"Application stopped successfully.\n";
        send(new_socket, (char*)successMsg.c_str(), successMsg.size() * sizeof(wchar_t), 0);
        std::cout << "Stop app successfully.\n";
    }
    else {
        std::wstring failureMsg = L"Failed to stop the application.\n";
        send(new_socket, (char*)failureMsg.c_str(), failureMsg.size() * sizeof(wchar_t), 0);
    }
}

// Hàm lấy danh sách dịch vụ
struct ServiceInfo {
    std::wstring serviceName;  // Tên dịch vụ (Service Name)
    std::wstring displayName; // Tên hiển thị (Display Name)
    std::wstring status;      // Trạng thái (Running, Stopped, v.v.)
};

std::wstring serviceStatusToString(DWORD dwCurrentState) {
    switch (dwCurrentState) {
    case SERVICE_STOPPED: return L"Stopped";
    case SERVICE_START_PENDING: return L"Starting...";
    case SERVICE_STOP_PENDING: return L"Stopping...";
    case SERVICE_RUNNING: return L"Running";
    case SERVICE_CONTINUE_PENDING: return L"Continue Pending";
    case SERVICE_PAUSE_PENDING: return L"Pause Pending";
    case SERVICE_PAUSED: return L"Paused";
    default: return L"Unknown";
    }
}

std::vector<ServiceInfo> getServiceList() {
    std::vector<ServiceInfo> serviceList;
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCManager) {
        std::wcerr << L"OpenSCManager failed. Error: " << GetLastError() << std::endl;
        return serviceList;
    }

    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;

    EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,          // Chỉ lấy dịch vụ (không lấy driver)
        SERVICE_STATE_ALL,      // Lấy tất cả dịch vụ (đang chạy và dừng)
        NULL,
        0,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL
    );

    std::vector<BYTE> buffer(bytesNeeded);
    LPBYTE lpBuffer = buffer.data();

    if (EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        lpBuffer,
        bytesNeeded,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        NULL)) {
        LPENUM_SERVICE_STATUS_PROCESS services = (LPENUM_SERVICE_STATUS_PROCESS)lpBuffer;
        for (DWORD i = 0; i < servicesReturned; i++) {
            ServiceInfo info;
            info.serviceName = services[i].lpServiceName;
            info.displayName = services[i].lpDisplayName;
            info.status = serviceStatusToString(services[i].ServiceStatusProcess.dwCurrentState); // Thêm trạng thái
            serviceList.push_back(info);
        }
    }
    else {
        std::wcerr << L"EnumServicesStatusEx failed. Error: " << GetLastError() << std::endl;
    }
    CloseServiceHandle(hSCManager);
    return serviceList;
}

std::string getServiceListStr(const std::vector<ServiceInfo>& serviceList) {
	std::string serviceListStr = "List of services:\n";
	for (size_t i = 0; i < serviceList.size(); ++i) {
		serviceListStr += std::to_string(i + 1) + ". " + std::string(serviceList[i].displayName.begin(), serviceList[i].displayName.end()) 
            + " - " + std::string(serviceList[i].status.begin(), serviceList[i].status.end()) + "\n";
	}
	return serviceListStr;
}
// Hàm khởi động dịch vụ
bool startService(const std::wstring& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        std::wcerr << L"OpenSCManager failed. Error: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hService) {
        std::wcerr << L"OpenService failed. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    if (!StartService(hService, 0, NULL)) {
        std::wcerr << L"StartService failed. Error: " << GetLastError() << std::endl;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

// Hàm tăt dịch vụ
bool handleStopService(const std::wstring& serviceName) {

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        std::wcerr << L"Không thể kết nối đến Service Control Manager. Lỗi: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        std::wcerr << L"Không thể mở dịch vụ: " << serviceName << L". Lỗi: " << GetLastError() << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS serviceStatus;
    if (ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        std::wcout << L"Dịch vụ " << serviceName << L" đang được dừng..." << std::endl;
        Sleep(1000); // Đợi một chút để đảm bảo dịch vụ đã dừng
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_NOT_ACTIVE) {
            std::wcout << L"Dịch vụ " << serviceName << L" đã tắt." << std::endl;
        }
        else {
            std::wcerr << L"Không thể dừng dịch vụ. Lỗi: " << error << std::endl;
        }
    }

    // Đóng handle
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

// Hàm lịch sử
std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

// Hàm lưu request vào file JSON
void saveRequestToFile(const std::string& clientName, const std::string& request) {
    std::ifstream inFile("requests.json");
    json requestData;

    // Đọc dữ liệu từ file nếu tồn tại
    if (inFile.is_open()) {
        inFile >> requestData;
        inFile.close();
    }

    // Tạo request mới
    json newRequest = {
        {"request", request},
        {"timestamp", getCurrentTimestamp()}
    };

    // Thêm request vào danh sách của client
    requestData[clientName].push_back(newRequest);

    // Ghi lại vào file
    std::ofstream outFile("requests.json");
    if (outFile.is_open()) {
        outFile << requestData.dump(4);
        outFile.close();
    }
    else {
        std::cerr << "Failed to open requests.json for writing!" << std::endl;
    }
}
std::string getRequestHistory(const std::string& clientName) {
    std::ifstream inFile("requests.json");
    json requestData;
    std::string history;

    // Đọc dữ liệu từ file
    if (inFile.is_open()) {
        inFile >> requestData;
        inFile.close();
    }
    else {
        return "Failed to open requests.json for reading!";
    }

    // Kiểm tra xem client có tồn tại không
    if (requestData.contains(clientName)) {
        history += "Request history for " + clientName + ":\n";
        for (const auto& req : requestData[clientName]) {
            history += "- " + req["request"].get<std::string>() +
                " (at " + req["timestamp"].get<std::string>() + ")\n";
        }
    }
    else {
        history = "No history found for client: " + clientName + "\n";
    }

    return history;
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
        memset(buffer, 0, sizeof(buffer));
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
        saveRequestToFile(sender, request);
		// Gửi danh sách các lệnh có thể thực thi
        if (request == "list") {
            const char* available_commands =
                "Available commands:\n"
                "1. shutdown\n"
                "2. screen capture\n"
                "3. webcam capture\n"
                "4. webcam record\n"
                "5. list files\n"
                "6. get file\n"
                "7. delete file\n"
                "8. list apps\n"
                "9. start app\n"
                "10. stop app\n"
                "11. list services\n"
                "12. start service\n"
                "13. stop service\n"
                "14. history\n";
            send(new_socket, available_commands, strlen(available_commands), 0);
            std::cout << "Sent list of available commands to client." << std::endl;
        }

        else if (request == "exit") {
            std::cout << "Client sent exit command. Closing connection..." << std::endl;
            break;
        }

        else if (request == "request access") {
			std::string response = "Access granted.";
            send(new_socket, response.c_str(), response.size() + 1, 0);
            std::cout << "Access granted from client.\n";
        }
     
        else if (request == "shutdown") {
            std::cout << "Shutdown command received, shutting down..." << std::endl;
            ShutdownSystem();
        }
	
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

            recordVideo("webcam_video.avi", duration);
			std::string response = "ok";
            send(new_socket, "ok", response.size() + 1, 0);
            handleSendFile("webcam_video.avi", new_socket);
        }

        else if (request == "list files") {
            std::vector<std::string> fileList = getFileList();
            if (fileList.empty()) {
                const char* MESS = "No files available.";
                send(new_socket, MESS, strlen(MESS), 0);
            }
            std::string fileListMessage = "Available files:\n";
            for (size_t i = 0; i < fileList.size(); ++i) {
                fileListMessage += std::to_string(i + 1) + ". " + fileList[i] + "\n";
            }
            send(new_socket, fileListMessage.c_str(), fileListMessage.size() + 1, 0);
			std::cout << "Sent list of files to client.\n";
        }

        else if (request == "get file") {
            std::vector<std::string> fileList = getFileList();
            if (fileList.empty()) {
                const char* MESS = "No files available to get.";
                send(new_socket, MESS, strlen(MESS), 0);               
            }
            std::string fileListMessage = "Available files:\n";
            for (size_t i = 0; i < fileList.size(); ++i) {
                fileListMessage += std::to_string(i + 1) + ". " + fileList[i] + "\n";
            }
            fileListMessage += "Enter file index you want to get: ";
            send(new_socket, fileListMessage.c_str(), fileListMessage.size() + 1, 0);
			recv(new_socket, buffer, BUFFER_SIZE, 0);
			int fileIndex = atoi(buffer) - 1;
			send(new_socket, fileList[fileIndex].c_str(), fileList[fileIndex].size() + 1, 0);
			std::string filePath = "./list file/" + fileList[fileIndex];
			handleSendFile(filePath, new_socket);
            
        }

        else if (request == "delete file") {
            std::vector<std::string> fileList = getFileList();
            if (fileList.empty()) {
                const char* MESS = "No files available to delete.";
                send(new_socket, MESS, strlen(MESS), 0);
            }   
            std::string fileListMessage = "Available files:\n";
            for (size_t i = 0; i < fileList.size(); ++i) {
                fileListMessage += std::to_string(i + 1) + ". " + fileList[i] + "\n";
            }
            fileListMessage += "Enter the file index to delete: ";
            send(new_socket, fileListMessage.c_str(), fileListMessage.size() + 1, 0);
            recv(new_socket, buffer, BUFFER_SIZE, 0);
            int fileIndex = atoi(buffer) - 1;
            if (fileIndex < 0 || fileIndex >= static_cast<int>(fileList.size())) {
                const char* MESS = "Invalid file number.";
                send(new_socket, MESS, strlen(MESS), 0);           
            }
            else if (handleDeleteFile(fileList[fileIndex].c_str())) {
				std::cout << "File deleted successfully." << std::endl;
                const char* MESS = "File deleted successfully.";
                send(new_socket, MESS, strlen(MESS), 0);
            }
            else {
				std::cout << "Failed to delete file." << std::endl;
                const char* MESS = "Failed to delete file.";
                send(new_socket, MESS, strlen(MESS), 0);
            }
        }

        else if (request == "list apps") {    
            std::vector<std::wstring> app_list = getListApps();
            std::string app_list_str = "Installed applications:\n";
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            for (const std::wstring& app : app_list) {
                std::string app_str = converter.to_bytes(app);
                app_list_str += app_str + "\n";
            }
			sendData(new_socket, app_list_str);
			std::cout << "Sent list of installed apps to client." << std::endl;
        }

        else if (request == "start app") {
            std::vector<std::wstring> app_list = getListApps();
			std::string app_list_str = "Choose an app to run:\n";
			for (int i = 0; i < app_list.size(); ++i) {
				app_list_str += std::string(app_list[i].begin(), app_list[i].end()) + "\n";              
			}
            app_list_str += "Choose app to run:\n";
			send(new_socket, app_list_str.c_str(), app_list_str.size() + 1, 0);
            std::cout << "Waiting app index from client...\n";
			char appIndexBuffer[10];
			int appIndexReceived = recv(new_socket, appIndexBuffer, sizeof(appIndexBuffer), 0);
			if (appIndexReceived <= 0) {
				std::cout << "Failed to receive app index from client." << std::endl;
				break;
			}			
			int appIndex = std::stoi(appIndexBuffer);
			bool Check = runApp(app_list, appIndex);
            if (Check) {
                const char* MESS = "Successfully start application ";
                send(new_socket, MESS, strlen(MESS), 0);
            }
            else {
                const char* MESS = "Cannot start application ";
                send(new_socket, MESS, strlen(MESS), 0);
            }
        }

        else if (request == "stop app") {
            handleStopApp(new_socket);
        }

        else if (request == "list services") {
            std::vector<ServiceInfo> serviceList = getServiceList();
			std::string serviceList_str = getServiceListStr(serviceList);
            send(new_socket, serviceList_str.c_str(), serviceList_str.size() + 1, 0);
            std::cout << "Sent list of services to client." << std::endl;
            }

        else if (request == "start service") {
            std::vector<ServiceInfo> serviceList = getServiceList();
			std::string serviceList_str = getServiceListStr(serviceList);
			serviceList_str += "Choose service to start:";
            send(new_socket, serviceList_str.c_str(), serviceList_str.size() + 1, 0);
            std::cout << "Waiting service index from client...\n";
            char serviceIndexBuffer[10];
            int serviceIndex = recv(new_socket, serviceIndexBuffer, sizeof(serviceIndexBuffer), 0);
            if (serviceIndex<= 0) {
                std::cout << "Failed to receive app index from client." << std::endl;
                break;
            }          
            int idx = std::stoi(serviceIndexBuffer);
            bool Check = startService(serviceList[idx-1].serviceName);
            if (Check) {
				std::cout << "Start service successfully\n";
                std::string MESS = "Start service successfully";
                send(new_socket, MESS.c_str(), MESS.size() + 1, 0);
            }
            else {
				std::cout << "Cannot start service\n";
                std::string MESS = "Cannot start service";
                send(new_socket, MESS.c_str(), MESS.size() + 1, 0);
            }         
        }

        else if (request == "stop service") {
            std::vector<ServiceInfo> serviceList = getServiceList();
			std::string serviceList_str = getServiceListStr(serviceList);
            serviceList_str = "Choose service to stop:";
            send(new_socket, serviceList_str.c_str(), serviceList_str.size() + 1, 0);
            std::cout << "Waiting service index from client...\n";
            char serviceIndexBuffer[10];
            int serviceIndex = recv(new_socket, serviceIndexBuffer, sizeof(serviceIndexBuffer), 0);
            if (serviceIndex <= 0) {
                std::cout << "Failed to receive app index from client." << std::endl;
                break;
            }
            int idx = std::stoi(serviceIndexBuffer);
            bool Check = startService(serviceList[idx-1].serviceName);
            if (Check) {
                const char* MESS = "Stop service successfully";
                send(new_socket, MESS, strlen(MESS), 0);
            }
            else {
                const char* MESS = "Cannot start service";
                send(new_socket, MESS, strlen(MESS), 0);
            }
        }
        
        else if (request == "history") {
            std::string response = "Your request history:\n";
			response += getRequestHistory(sender);
			send(new_socket, response.c_str(), response.size() + 1, 0);
			std::cout << "Sent request history to client.\n";
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


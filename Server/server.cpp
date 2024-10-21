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
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
const int BUFFER_SIZE = 1024;
// Các hàm chức năng
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
void handleSendFile(const std::string& file_path, SOCKET new_socket) {
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
        std::cout << "Sent image to client." << std::endl;
    }
    else {
        std::cout << "Failed to open image file." << std::endl;
    }
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

// Helper function to find the executable in a directory
std::wstring findExecutableInDirectory(const std::wstring& directory) {
    std::wstring searchPath = directory + L"\\*.exe";
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return directory + L"\\" + findData.cFileName;
    }
    return L"";
}

// Hàm lấy danh sách các ứng dụng đã cài đặt
std::vector<std::wstring> getListApps() {
    std::vector<std::wstring> app_list;
    std::set<std::wstring> unique_apps;  // Use set to prevent duplicates and maintain consistency
    HKEY hUninstallKey = NULL;
    HKEY hAppKey = NULL;

    // Define registry paths to check
    struct RegPath {
        HKEY hKey;
        const wchar_t* path;
    };

    RegPath paths[] = {
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
        {HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}
    };

    for (const auto& regPath : paths) {
        if (RegOpenKeyEx(regPath.hKey, regPath.path, 0, KEY_READ, &hUninstallKey) != ERROR_SUCCESS) {
            continue;
        }

        // Get the number of subkeys
        DWORD subKeyCount = 0;
        DWORD maxSubKeyLen = 0;
        if (RegQueryInfoKey(hUninstallKey, NULL, NULL, NULL, &subKeyCount, &maxSubKeyLen,
            NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            RegCloseKey(hUninstallKey);
            continue;
        }

        // Allocate buffer for subkey name
        std::vector<TCHAR> appName(maxSubKeyLen + 1);

        // Enumerate all subkeys
        for (DWORD i = 0; i < subKeyCount; i++) {
            DWORD nameSize = maxSubKeyLen + 1;
            LONG result = RegEnumKeyEx(hUninstallKey, i, appName.data(), &nameSize,
                NULL, NULL, NULL, NULL);

            if (result == ERROR_SUCCESS) {
                if (RegOpenKeyEx(hUninstallKey, appName.data(), 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
                    TCHAR displayName[256] = { 0 };
                    TCHAR installLocation[1024] = { 0 };
                    TCHAR displayIcon[1024] = { 0 };
                    DWORD displayNameSize = sizeof(displayName);
                    DWORD installLocationSize = sizeof(installLocation);
                    DWORD displayIconSize = sizeof(displayIcon);

                    // Get DisplayName
                    if (RegQueryValueEx(hAppKey, L"DisplayName", NULL, NULL,
                        (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
                        std::wstring executablePath;
                        bool hasValidPath = false;

                        // Try DisplayIcon first
                        if (RegQueryValueEx(hAppKey, L"DisplayIcon", NULL, NULL,
                            (LPBYTE)displayIcon, &displayIconSize) == ERROR_SUCCESS) {
                            executablePath = displayIcon;
                            size_t commaPos = executablePath.find(L',');
                            if (commaPos != std::wstring::npos) {
                                executablePath = executablePath.substr(0, commaPos);
                            }
                            hasValidPath = true;
                        }
                        // Try InstallLocation if DisplayIcon failed
                        else if (RegQueryValueEx(hAppKey, L"InstallLocation", NULL, NULL,
                            (LPBYTE)installLocation, &installLocationSize) == ERROR_SUCCESS) {
                            std::wstring path = installLocation;
                            if (!path.empty()) {
                                WIN32_FIND_DATA findData;
                                std::wstring searchPath = path + L"\\*.exe";
                                HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
                                if (hFind != INVALID_HANDLE_VALUE) {
                                    executablePath = path + L"\\" + findData.cFileName;
                                    hasValidPath = true;
                                    FindClose(hFind);
                                }
                            }
                        }

                        if (hasValidPath) {
                            // Clean up the path
                            if (!executablePath.empty()) {
                                if (executablePath.front() == L'"' && executablePath.back() == L'"') {
                                    executablePath = executablePath.substr(1, executablePath.length() - 2);
                                }

                                // Only add if the executable exists
                                if (GetFileAttributes(executablePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                    std::wstring appInfo = std::wstring(displayName) + L" - " + executablePath;
                                    unique_apps.insert(appInfo);
                                }
                            }
                        }
                    }
                    RegCloseKey(hAppKey);
                }
            }
        }
        RegCloseKey(hUninstallKey);
    }

    // Convert set to vector
    app_list.assign(unique_apps.begin(), unique_apps.end());
    return app_list;
}

// Hàm chạy ứng dụng
void runApp(const std::vector<std::wstring>& app_list, int appIndex) {
    if (appIndex < 0 || appIndex >= app_list.size()) {
        std::wcout << L"Invalid app index." << std::endl;
        return;
    }

    const std::wstring& appInfo = app_list[appIndex];
    size_t separatorPos = appInfo.find(L" - ");
    if (separatorPos == std::wstring::npos) {
        std::wcout << L"Invalid application information." << std::endl;
        return;
    }

    std::wstring appPath = appInfo.substr(separatorPos + 3);

    // Remove any surrounding quotes if present
    if (appPath.front() == L'"' && appPath.back() == L'"') {
        appPath = appPath.substr(1, appPath.length() - 2);
    }

    // Verify the executable exists
    if (GetFileAttributes(appPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcout << L"Executable not found: " << appPath << std::endl;
        return;
    }

    // Use ShellExecuteEx with "runas" verb to handle elevation
    SHELLEXECUTEINFO ShExecInfo = { 0 };
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = L"runas";  // Request elevation
    ShExecInfo.lpFile = appPath.c_str();
    ShExecInfo.lpParameters = L"";
    ShExecInfo.lpDirectory = NULL;
    ShExecInfo.nShow = SW_SHOW;
    ShExecInfo.hInstApp = NULL;

    if (ShellExecuteEx(&ShExecInfo)) {
        std::wcout << L"Running application: " << appPath << std::endl;
        if (ShExecInfo.hProcess) {
            CloseHandle(ShExecInfo.hProcess);
        }
    }
    else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            std::wcout << L"User declined elevation request." << std::endl;
        }
        else {
            std::wcout << L"Failed to run the application: " << appPath << L" (Error: " << error << L")" << std::endl;
        }
    }
}
// Function to close an application by its process name
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
        buffer[recv_size] = '\0'; // Đảm bảo chuỗi kết thúc bằng null
       
        // Xử lý lệnh từ client
        std::cout << "Message from client: " << buffer << std::endl;
        std::string message(buffer);
        size_t delimiterPos = message.find(": ");
        std::string sender = "";
        std::string request = "";
        if (delimiterPos != std::string::npos) {
            sender = message.substr(0, delimiterPos); // Extract sender
            request = message.substr(delimiterPos + 2); // Extract request
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
                "10. Run app\n";
            send(new_socket, available_commands, strlen(available_commands), 0);
            std::cout << "Sent list of available commands to client." << std::endl;
        }

        // Xử lý lệnh exit
        else if (request == "exit") {
            std::cout << "Client sent exit command. Closing connection..." << std::endl;
            break; // Thoát vòng lặp khi client gửi lệnh "exit"
        }

        // Xử lý lệnh shutdown
        else if (request == "shutdown") {
            std::cout << "Shutdown command received, shutting down..." << std::endl;
            ShutdownSystem();
        }

        // Xử lý lệnh log in
        else if (request == "log in") {}

		// Xử lý lệnh log out
		else if (request == "log out") {}

		// Xử lý lệnh screen capture
        else if (request == "screen capture") {
            // Chụp ảnh từ màn hình và lưu vào file
            TakeScreenshot("D:\\Hp\\Pictures\\Screenshots\\screenshot.bmp");       
			handleSendFile("D:\\Hp\\Pictures\\Screenshots\\screenshot.bmp", new_socket);
        }

        // Xử lý lệnh webcam capture
		else if (request == "webcam capture") {

			// Chụp ảnh từ webcam và lưu vào file
			CaptureWebcamImage("D:\\Hp\\Pictures\\Screenshots\\webcam_image.jpg");
			handleSendFile("D:\\Hp\\Pictures\\Screenshots\\webcam_image.jpg", new_socket);
		}

		// Xử lý lệnh webcam record
        else if (request == "webcam record") {
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

        // Xử lý lệnh getFile
        else if (request == "getFile") {
            int valread = recv(new_socket, buffer, BUFFER_SIZE, 0);
            std::string filepath(buffer);

            std::cout << "Yêu cầu file: " << filepath << std::endl;

            std::ifstream file(filepath, std::ios::binary);
            if (file) {
                while (!file.eof()) {
                    file.read(buffer, BUFFER_SIZE);
                    send(new_socket, buffer, file.gcount(), 0);
                }
                file.close();
                std::cout << "File đã được gửi" << std::endl;
            }
            else {
                const char* msg = "File không tồn tại";
                send(new_socket, msg, strlen(msg), 0);
            }

        }

        // Xử lý lệnh getListApps
		else if (request == "getListApps") {
            std::vector<std::wstring> app_list = getListApps();
            std::string app_list_str = "Installed applications:\n";

            // Converter for wstring to string
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

            for (const std::wstring& app : app_list) {
                // Convert wstring to string
                std::string app_str = converter.to_bytes(app);
                app_list_str += app_str + "\n"; // Add each app to the list with a newline
            }

            // Send the list to the client
            send(new_socket, app_list_str.c_str(), app_list_str.size() + 1, 0);
            std::cout << "Sent list of installed applications to client." << std::endl;
		}
        
        // Xử lý lệnh runApp
        else if (request == "runApp") {
			std::vector<std::wstring> app_list = getListApps();

			// Gửi danh sách ứng dụng cho client
			std::string app_list_str = "Choose an app to run:\n";
			for (int i = 0; i < app_list.size(); ++i) {
				app_list_str += std::to_string(i) + ". " + std::string(app_list[i].begin(), app_list[i].end()) + "\n";
			}
			send(new_socket, app_list_str.c_str(), app_list_str.size() + 1, 0);

			// Nhận chỉ số ứng dụng từ client
			char appIndexBuffer[10];
			int appIndexReceived = recv(new_socket, appIndexBuffer, sizeof(appIndexBuffer), 0);
			if (appIndexReceived <= 0) {
				std::cout << "Failed to receive app index from client." << std::endl;
				break;
			}
			appIndexBuffer[appIndexReceived] = '\0';
			int appIndex = std::stoi(appIndexBuffer);

			// Chạy ứng dụng
			runApp(app_list, appIndex);
        }
        
        // Xử lý lệnh closeApp
        else if (request == "closeApp") {
            std::vector<std::wstring> app_list = getListApps();
            handleCloseApp(app_list, new_socket);
        }
		// Xử lý các lệnh không hợp lệnh
        else {
            send(new_socket, response, strlen(response), 0);
            std::cout << "No valid" << std::endl;
        }
    }

    // 8. Đóng kết nối
    closesocket(new_socket);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}


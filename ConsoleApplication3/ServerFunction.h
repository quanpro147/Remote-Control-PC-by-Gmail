#pragma once
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include <QtNetwork/qtcpsocket.h>
#include<QtNetwork/qtcpserver.h>
#include <QtCore/QObject>
#include<QtCore/qthread.h>
#include<string>
#include<iostream>
#include<windows.h>
#include <opencv2/opencv.hpp> 
#include<QtCore/qfile.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp> 
#include<stdio.h>
#include <psapi.h>
#include <locale>
#include <codecvt>
#include <tlhelp32.h>
#include <psapi.h>
#include <gdiplus.h>
#include <cstdio>
#include <winsvc.h>

#include<fstream>
#pragma comment(lib, "gdiplus.lib")
const LPCWSTR AppCommand[] = { L"chrome.exe",L"winword.exe",L"excel.exe",L"msedge.exe",L"mspaint.exe",L"explorer.exe",L"notepad.exe",L"calc.exe",L"snippingtool.exe",L"powerpnt.exe" ,L"microsoft.windows.camera" };
const std::wstring AppList[]= { L"chrome",L"Word",L"Excel",L"Edge",L"Paint",L"Explorer",L"Notepad",L"Calculator",L"snipping tool",L"Power Point",L"camera" };
const char AvaiableCommand[] = "Available commands:\n"
                "1. Shutdown PC\n"
                "2. Restart PC\n"
                "3. Get list apps\n"
                "4. Run app\n"
                "5. Screen capture\n"
                "6. Webcam capture\n"
                "7. Webcam record\n"
                "8. Get file\n"
                "9. Close app\n"
                "10. List services\n";
const char AvaiableApp[] = "0. chrome\n""1. Word\n""2. Excel\n""3. Edge\n""4. Paint\n""5. Explorer\n""6. Notepad\n""7. Calculator\n""8. snipping tool\n""9. Power Point\n""10. camera\n";
struct ProcessInfo {
    DWORD processID;
    std::wstring exeName;
};
inline BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    DWORD windowProcessID;
    GetWindowThreadProcessId(hwnd, &windowProcessID);


    if (windowProcessID == (DWORD)lParam) {

        if (IsWindowVisible(hwnd)) {
            return FALSE;
        }
    }
    return TRUE;
}

inline bool HasVisibleWindow(DWORD processID) {
    return !EnumWindows(EnumWindowsCallback, (LPARAM)processID);
}
inline bool StartServiceByName(const std::wstring& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName.c_str(), SERVICE_START);
    if (!hService) {
        std::cerr << "Failed to open service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    if (!StartService(hService, 0, nullptr)) {
        std::cerr << "Failed to start service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return false;
    }

    std::wcout << L"Service " << serviceName << L" started successfully." << std::endl;

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}


inline bool StopServiceByName(const std::wstring& serviceName) {
    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenService(hSCManager, serviceName.c_str(), SERVICE_STOP);
    if (!hService) {
        std::cerr << "Failed to open service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    SERVICE_STATUS serviceStatus;
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        std::cerr << "Failed to stop service. Error: " << GetLastError() << std::endl;
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        return false;
    }

    std::wcout << L"Service " << serviceName << L" stopped successfully." << std::endl;

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

inline std::vector<ProcessInfo> GetRunningApps() {
    std::vector<ProcessInfo> apps;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot.\n";
        return apps;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (HasVisibleWindow(pe32.th32ProcessID)) {

                apps.push_back({ pe32.th32ProcessID, pe32.szExeFile });
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return apps;
}


inline std::vector<ProcessInfo> GetProcessList() {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    std::vector<ProcessInfo> processList;


    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to take a snapshot of processes." << std::endl;
        return processList;
    }


    pe32.dwSize = sizeof(PROCESSENTRY32);


    if (!Process32First(hProcessSnap, &pe32)) {
        std::cerr << "Failed to get the first process." << std::endl;
        CloseHandle(hProcessSnap);
        return processList;
    }


    do {
        ProcessInfo pInfo;
        pInfo.processID = pe32.th32ProcessID;
        pInfo.exeName = pe32.szExeFile;
        processList.push_back(pInfo);
    } while (Process32Next(hProcessSnap, &pe32));


    CloseHandle(hProcessSnap);

    return processList;
}
inline void SendFile(QTcpSocket* socket, const QString& file_path) {
    if (!socket || !socket->isOpen()) {
        qDebug() << "Socket is not connected.";
        return;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file:" << file.errorString();
        return;
    }

    // 
    quint32 fileSize = static_cast<quint32>(file.size());

    // 
    QByteArray sizeBuffer;
    QDataStream sizeStream(&sizeBuffer, QIODevice::WriteOnly);
    sizeStream << fileSize;

    if (socket->write(sizeBuffer) == -1) {
        qDebug() << "Failed to send file size:" << socket->errorString();
        file.close();
        return;
    }


    const qint64 chunkSize = 1024; // 
    QByteArray buffer(chunkSize, 0); // 
    qint64 bytesSent = 0;

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), chunkSize); // 
        if (bytesRead == -1) {
            qDebug() << "Failed to read from file:" << file.errorString();
            file.close();
            return;
        }

        qint64 sent = 0;
        int j = 0;
        while (sent < bytesRead) {
            qint64 result = socket->write(buffer.constData() + sent, bytesRead - sent); // 
            if (result == -1) {
                qDebug() << "Failed to send file data:" << socket->errorString();
                file.close();
                return;
            }
            sent += result;
            j++;
            std::cout << j;
        }

        bytesSent += bytesRead;
        QThread::msleep(50);
    }

    socket->flush(); //
    file.close();
    qDebug() << "Sent file to client successfully. Total bytes sent:" << bytesSent;
}
struct ServiceInfo{
    std::wstring serviceName;
    std::wstring displayName;
    DWORD serviceStatus;
};

inline std::wstring ServiceStatusToString(DWORD status) {
    switch (status) {
    case SERVICE_STOPPED: return L"Stopped";
    case SERVICE_START_PENDING: return L"Start Pending";
    case SERVICE_STOP_PENDING: return L"Stop Pending";
    case SERVICE_RUNNING: return L"Running";
    case SERVICE_CONTINUE_PENDING: return L"Continue Pending";
    case SERVICE_PAUSE_PENDING: return L"Pause Pending";
    case SERVICE_PAUSED: return L"Paused";
    default: return L"Unknown";
    }
}


inline std::vector<ServiceInfo> GetServicesList() {
    std::vector<ServiceInfo> services;

    SC_HANDLE hSCManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCManager) {
        std::cerr << "Failed to open Service Control Manager. Error: " << GetLastError() << std::endl;
        return services;
    }

    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;

   
    EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        nullptr,
        0,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        nullptr
    );

    DWORD bufferSize = bytesNeeded;
    LPBYTE buffer = new BYTE[bufferSize];

    if (EnumServicesStatusEx(
        hSCManager,
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        buffer,
        bufferSize,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        nullptr
    )) {
        LPENUM_SERVICE_STATUS_PROCESS servicesArray = (LPENUM_SERVICE_STATUS_PROCESS)buffer;

        for (DWORD i = 0; i < servicesReturned; ++i) {
            ServiceInfo serviceInfo;
            serviceInfo.serviceName = servicesArray[i].lpServiceName;
            serviceInfo.displayName = servicesArray[i].lpDisplayName;
            serviceInfo.serviceStatus = servicesArray[i].ServiceStatusProcess.dwCurrentState;

            services.push_back(serviceInfo);
        }
    }
    else {
        std::cerr << "Failed to enumerate services. Error: " << GetLastError() << std::endl;
    }

    delete[] buffer;
    CloseServiceHandle(hSCManager);

    return services;
}

inline QString HandleRunApp(QTcpSocket* Sock) {
    QByteArray Data = QByteArray(AvaiableApp);
    Sock->write(Data);
    QString S = "Enter the Index you want to run : ";
    Sock->write(S.toUtf8());
    char Buffer[1024];
    qint64 Bytes = Sock->read(Buffer, 1024);
    int Index = atoi(Buffer);
    return "HELLO";

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
class ServerFunction:public QObject{

public:

void SaveScreenshotToJPG(const std::wstring file_path, ULONG quality = 90) {
    // 
    GdiplusInitializer gdiplusInit;
    //
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // 
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(screenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(memDC, hBitmap);

    // 
    BitBlt(memDC, 0, 0, width, height, screenDC, 0, 0, SRCCOPY);

    // 
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hBitmap, NULL);

    // 
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

    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

   
    bitmap->Save(file_path.c_str(), &encoderClsid, &encoderParams);


    delete bitmap;
    SelectObject(memDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);
}
inline void TakeScreenshot(const std::string file_path, ULONG quality = 90) {
    std::cout << "HELLO" << std::endl;
    std::wstring wide_path(file_path.begin(), file_path.end());
    std::wcout << wide_path << std::endl;
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
bool RecordVideoFromWebcam(const std::string output_file, int duration_in_seconds) {
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
    int total_frames = fps * duration_in_seconds; //

    std::cout << "Recording video for " << duration_in_seconds << " seconds..." << std::endl;

    for (int i = 0; i < total_frames; ++i) {
        cap >> frame;  //
        if (frame.empty()) {
            std::cout << "Failed to capture image" << std::endl;
            break;
        }
        // 
        video.write(frame);
        // 
        cv::imshow("Webcam", frame);
        // 
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
};

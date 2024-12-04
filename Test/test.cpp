#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

void GetInstalledApps(HKEY hKeyRoot, const std::wstring& subKeyPath, std::vector<std::wstring>& apps) {
    HKEY hKey;
    if (RegOpenKeyEx(hKeyRoot, subKeyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        wchar_t keyName[256];
        DWORD keyNameSize = sizeof(keyName) / sizeof(wchar_t);

        while (RegEnumKeyEx(hKey, index, keyName, &keyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            HKEY hSubKey;
            if (RegOpenKeyEx(hKey, keyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                wchar_t displayName[256];
                DWORD displayNameSize = sizeof(displayName);
                DWORD type;
                if (RegQueryValueEx(hSubKey, L"DisplayName", NULL, &type, (LPBYTE)displayName, &displayNameSize) == ERROR_SUCCESS) {
                    apps.push_back(displayName);
                }
                RegCloseKey(hSubKey);
            }
            keyNameSize = sizeof(keyName) / sizeof(wchar_t);  // Reset size for the next key
            ++index;
        }
        RegCloseKey(hKey);
    }
}

int main() {
    std::vector<std::wstring> apps;

    // Lấy ứng dụng từ HKEY_LOCAL_MACHINE (64-bit)
    GetInstalledApps(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);

    // Lấy ứng dụng từ HKEY_LOCAL_MACHINE (32-bit, WOW6432Node)
    GetInstalledApps(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);

    // Lấy ứng dụng từ HKEY_CURRENT_USER
    GetInstalledApps(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", apps);
    
    // Hiển thị danh sách ứng dụng
    std::wcout << L"Installed Applications:\n";
    for (const auto& app : apps) {
        std::wcout << L"- " << app << L"\n";
    }

    return 0;
}


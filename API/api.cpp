#define CURL_STATICLIB

#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include "curl/curl.h"
#include "curl/easy.h"
#include <string>
#include "json.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <thread>
#include <set>

using json = nlohmann::json;
#define PORT 8080

// Hàm callback để xử lý dữ liệu phản hồi
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);  // Thêm dữ liệu vào chuỗi userp
    return size * nmemb;  // Trả về kích thước dữ liệu đã xử lý
}

// Hàm để lấy mã xác thực
std::string getAuthorizationCode(const std::string& clientId, const std::string& redirectUri) {
    // Tạo URL xác thực cho người dùng
    std::string authUrl = "https://accounts.google.com/o/oauth2/v2/auth?response_type=code"
        "&client_id=" + clientId +
        "&redirect_uri=" + redirectUri +
        "&scope=https://www.googleapis.com/auth/gmail.readonly" +
        "&access_type=offline";

    std::cout << "Mở trình duyệt và truy cập vào: " << authUrl << std::endl;  // Hướng dẫn người dùng mở trình duyệt
    std::cout << "Nhập mã xác thực (authorization code): ";

    std::string code;
    std::cin >> code;  // Nhận mã xác thực từ người dùng
    return code;  // Trả về mã xác thực
}

// Hàm lấy access token với refresh token
std::pair<std::string, std::string> getAccessToken(const std::string& clientId, const std::string& clientSecret,
    const std::string& redirectUri, const std::string& authCode) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();  // Khởi tạo CURL
    if (curl) {
        std::string postFields = "code=" + authCode +
            "&client_id=" + clientId +
            "&client_secret=" + clientSecret +
            "&redirect_uri=" + redirectUri +
            "&grant_type=authorization_code";

        curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    json jsonData = json::parse(readBuffer);
    std::string accessToken = jsonData["access_token"];  // Lấy access token
    std::string refreshToken = jsonData.value("refresh_token", "");  // Lấy refresh token nếu có

    return { accessToken, refreshToken };  // Trả về cặp access token và refresh token
}

// Hàm để lấy access token mới từ refresh token
std::string getAccessTokenWithRefreshToken(const std::string& clientId, const std::string& clientSecret,
    const std::string& refreshToken) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        std::string postFields = "client_id=" + clientId +
            "&client_secret=" + clientSecret +
            "&refresh_token=" + refreshToken +
            "&grant_type=refresh_token";

        curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    }

    json jsonData = json::parse(readBuffer);
    return jsonData["access_token"];  // Trả về access token mới
}

//4/0AVG7fiSed0_Rk8OEhAvAedSzfE2m_GYlhu8BFii425nD-AFs4sQDIcmnv_Bna2RbcB2-uA&scope=https://www.googleapis.com/auth/gmail.readonly



std::string makeGetRequest(const std::string& url, const std::string& access_token) {
    CURL* curl = curl_easy_init();
    std::string response_string;

    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return response_string;
}
std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        }
        else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}
std::string getCurrentDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm_local;

#ifdef _WIN32
    localtime_s(&tm_local, &t);
#else
    localtime_r(&t, &tm_local);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_local, "%Y/%m/%d");
    return oss.str();
}

void getGmailMessages(const std::string& access_token, const std::string& after_time, std::set<std::string>& processed_ids) {
    std::string query = "after:" + after_time;
    std::string encoded_query = urlEncode(query);
    std::string messages_url = "https://gmail.googleapis.com/gmail/v1/users/me/messages?q=" + encoded_query;

    std::string response = makeGetRequest(messages_url, access_token);

    json messages_json = json::parse(response);

    if (messages_json.contains("messages") && messages_json["messages"].is_array()) {
        for (const auto& message : messages_json["messages"]) {
            std::string message_id = message["id"];

            // Kiểm tra xem tin nhắn đã được xử lý chưa
            if (processed_ids.find(message_id) == processed_ids.end()) {
                std::string message_url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + message_id;
                std::string message_response = makeGetRequest(message_url, access_token);

                json email_data = json::parse(message_response);

                std::string subject, sender;

                for (const auto& header : email_data["payload"]["headers"]) {
                    if (header["name"] == "Subject") {
                        subject = header["value"];
                    }
                    else if (header["name"] == "From") {
                        sender = header["value"];
                    }
                }

                std::cout << "New email received:" << std::endl;
                std::cout << "Subject: " << subject << std::endl;
                std::cout << "From: " << sender << std::endl;
                std::cout << "---" << std::endl;

                // Đánh dấu tin nhắn đã được xử lý
                processed_ids.insert(message_id);
            }
        }
    }
}

int main() {
    // A. Lấy access token
    //std::string clientId = "612933153793-1teill41m3i7t1qkit497uh6q8nkkhmc.apps.googleusercontent.com";          
    //std::string clientSecret = "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"; 
    //std::string redirectUri = "http://localhost:8080";   
    //// Bước 1: Lấy mã xác nhận từ người dùng
    //std::string authCode = getAuthorizationCode(clientId, redirectUri);
    //std::cout << "Mã xác nhận nhận được: " << authCode << std::endl;
    //// Bước 2: Lấy access token và refresh token từ mã xác nhận
    //auto tokens = getAccessToken(clientId, clientSecret, redirectUri, authCode);
    //std::string accessToken = tokens.first;
    //std::string refreshToken = tokens.second;
    //if (!accessToken.empty()) {
    //    std::cout << "Access Token: " << accessToken << std::endl;  // In access token
    //    //std::cout << "Refresh Token: " << refreshToken << std::endl;  // In refresh token
    //// Bước 3: Sử dụng refresh token để lấy access token mới
    //std::string newAccessToken = getAccessTokenWithRefreshToken(clientId, clientSecret, refreshToken);
    //std::cout << "Access Token mới: " << newAccessToken << std::endl;  // In access token mới
    //}
    //else {
    //    std::cerr << "Không thể lấy Access Token." << std::endl;
    //}
    std::string accessToken = "ya29.a0AcM612zkcDy98QXqOL36nTYQhlQHC-9J_cKHBUZbtjrkDFcCQ4r019TiZVzsHYVdCQ-QLxZa728T_YLAXvAqaDDvAwZTp3zLcY7CJ51IDC16XadwkAxaLj9Ztrjd_3LBQrPh2S74BfFQZArwUBV8z0yWafsFf8XWVtfTg1bSaCgYKAccSARESFQHGX2Miws16HBs6PVo-qnPnhIS94Q0175";
    // B. Đọc email
    std::string current_date = getCurrentDate();
    std::set<std::string> processed_ids;
    std::cout << "Monitoring for new emails. Press Ctrl+C to stop." << std::endl;
    while (true) {
        getGmailMessages(accessToken, current_date, processed_ids);
        // Đợi 5 giây trước khi kiểm tra lại
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    
    return 0;
}
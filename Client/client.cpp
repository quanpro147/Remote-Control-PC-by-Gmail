#define CURL_STATICLIB

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h> 
#include <string>
#include <fstream>
#include "curl/curl.h"
#include "json.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <thread>
#include <set>
#include <memory>
#include <vector>

#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "advapi32.lib")

using json = nlohmann::json;
const int BUFFER_SIZE = 1024;


static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

class GmailClient {
private:
    static constexpr int PORT = 8080;
    static constexpr int BUFFER_SIZE = 1024;
    bool isFirstRun;
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    std::string accessToken;
    std::set<std::string> processedIds;
    std::chrono::system_clock::time_point lastCheckTime;
    SOCKET sock;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    // Add these functions inside the GmailClient class as private methods:
private:
    std::string getQueryTime() const {
        auto now = std::chrono::system_clock::now();
        // Update lastCheckTime only after successfully processing messages
        auto time_t = std::chrono::system_clock::to_time_t(lastCheckTime);
        std::tm tm_local;

        #ifdef _WIN32
                localtime_s(&tm_local, &time_t);
        #else
                localtime_r(&time_t, &tm_local);
        #endif

        // Format using Unix timestamp (epoch seconds)
        std::ostringstream ss;
        ss << time_t;
        return ss.str();
    }

    std::string processNewEmails() {
        try {
            std::string queryTime = getQueryTime();
            // Use Unix timestamp directly in the query
            std::string encodedQuery = "after:" + queryTime;
            std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=10&q=" + encodedQuery;

            std::cout << "\n=== Checking for new emails ===\n";

            std::string response = makeGetRequest(url);
            
            json messages = json::parse(response);
            std::string command = "no command";

            if (messages.contains("messages") && messages["messages"].is_array()) {             
                for (const auto& message : messages["messages"]) {
                    std::string messageId = message["id"];                   
                    if (processedIds.find(messageId) == processedIds.end()) {
                        command = processMessage(messageId);
                        processedIds.insert(messageId);
                        std::cout << "New message processed, command: " << command << std::endl;
                    }                
                }
            }   
            if (command == "no command") {
                std::cout << "No new messages have found\n";
            }
            return command;
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing emails: " << e.what() << std::endl;
            refreshAccessToken();
            return "invalid";
        }
    }

    std::string processMessage(const std::string& messageId) {
        try {
            std::string messageUrl = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + messageId;
            std::string messageResponse = makeGetRequest(messageUrl);

            json emailData = json::parse(messageResponse);
            std::string subject, sender;

            // Extract sender and subject from headers
            for (const auto& header : emailData["payload"]["headers"]) {
                if (header["name"] == "Subject") {
                    subject = header["value"];
                }
                else if (header["name"] == "From") {
                    sender = header["value"];
                    // Extract email address from "Name <email@domain.com>" format
                    size_t start = sender.find('<');
                    size_t end = sender.find('>');
                    if (start != std::string::npos && end != std::string::npos) {
                        sender = sender.substr(start + 1, end - start - 1);
                    }
                }
            }

            // Format and send command to server
            std::string command = sender + ": " + subject;

            // Debug output
            std::cout << "Processing new email: " << command << std::endl;

            send(sock, command.c_str(), command.length(), 0);

            std::cout << "Command sent to server: " << command << std::endl;
            std::cout << "---" << std::endl;
			return command;
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing message " << messageId << ": " << e.what() << std::endl;
        }
    }

    std::string urlEncode(const std::string& value) const {
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

    std::string makeGetRequest(const std::string& url) const {
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        std::string response;

        if (curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

            curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl.get());
            if (res != CURLE_OK) {
                throw std::runtime_error(std::string("Curl request failed: ") + curl_easy_strerror(res));
            }

            curl_slist_free_all(headers);
        }

        return response;
    }

    void refreshAccessToken() {
        std::string postFields = "client_id=" + clientId +
            "&client_secret=" + clientSecret +
            "&refresh_token=" + refreshToken +
            "&grant_type=refresh_token";
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        std::string response;
        if (curl) {
            curl_easy_setopt(curl.get(), CURLOPT_URL, "https://oauth2.googleapis.com/token");
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, postFields.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
            CURLcode res = curl_easy_perform(curl.get());
            if (res != CURLE_OK) {
                throw std::runtime_error(std::string("Token refresh failed: ") + curl_easy_strerror(res));
            }
        }
        json jsonData = json::parse(response);
        accessToken = jsonData["access_token"];
    }

    void handleServerCommand(const std::string& command) {
        try {      

            // Extract the request part after ": "
            std::string request = "invalid";
            size_t colonPos = command.find(": ");
            if (colonPos != std::string::npos && colonPos + 2 < command.length()) {
                request = command.substr(colonPos + 2); // +2 to skip ": "
            }
            else if (command == "invalid" || command == "no command") {
                request = command;
            }

            std::cout << "Parsed request: " << request << std::endl;  // Debug output
            // Handle the extracted request
            if (request == "exit") {
                throw std::runtime_error("Shutdown requested");
            }
            else if (request == "list" || request == "screen capture" || request == "webcam capture" || request == "webcam record" ||
                request == "shutdown" || request == "getFile" || request == "getListApps" || request == "runApp" || request == "closeApp") {
                 
                handleServerResponse(request);
                              
            }
            else if (request == "no command") {
                // Do nothing for "no command"
                return;
            }
            else if (request == "invalid") {
                // Do nothing for "invalid"
                return;
            }
            else {
                std::cout << "Invalid command: " << request << std::endl;
            }
        }
        catch (const std::bad_alloc& e) {
            std::cout << "Memory allocation error: " << e.what() << std::endl;
        }
        catch (const std::exception& e) {
            std::cout << "Error in handleServerCommand: " << e.what() << std::endl;
        }
    }

    void handleServerResponse(const std::string& command) {
        if (command == "list" || command == "getListApps") {
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
        else if (command == "runApp"){
            handlerunApp();
        }
        else if (command == "closeApp") {
			handlecloseApp();
        }
        sendEmailToOriginalSender();
    }

    void receiveFile(const std::string& filename) {
        size_t fileSize;
        if (recv(sock, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0) != sizeof(fileSize)) {
            throw std::runtime_error("Failed to receive file size");
        }
        std::vector<char> buffer(fileSize);
        if (recv(sock, buffer.data(), fileSize, 0) != fileSize) {
            throw std::runtime_error("Failed to receive file data");
        }

        std::ofstream outFile(filename, std::ios::binary);
        outFile.write(buffer.data(), fileSize);
        std::cout << "File saved as " << filename << std::endl;
    }

    void handlerunApp() {
        std::vector<char> buffer(BUFFER_SIZE);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received) << std::endl;
		int appIndex;
		std::cout << "Enter the index of the app you want to run: ";
		std::cin >> appIndex;
		send(sock, std::to_string(appIndex).c_str(), std::to_string(appIndex).size() + 1, 0);
    }

    void handlecloseApp() {
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
        std::vector<char> buffer(BUFFER_SIZE);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received);
        std::string duration;
        std::getline(std::cin, duration);
        send(sock, duration.c_str(), duration.size() + 1, 0);
        receiveFile("recording.avi");
    }

    void sendEmailToOriginalSender() {
        try {
            std::cout << "Starting sendEmailToOriginalSender..." << std::endl;

            // Check for processed emails
            if (processedIds.empty()) {
                std::cout << "No processed IDs found, returning..." << std::endl;
                return;
            }

            std::string lastMessageId = *processedIds.rbegin();
            std::cout << "Processing last message ID: " << lastMessageId << std::endl;

            std::string messageUrl = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + lastMessageId;
            std::cout << "Fetching message details..." << std::endl;

            std::string messageResponse = makeGetRequest(messageUrl);
            std::cout << "Received message response length: " << messageResponse.length() << std::endl;

            json emailData;
            try {
                emailData = json::parse(messageResponse);
                std::cout << "Successfully parsed JSON response" << std::endl;
            }
            catch (const json::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << "\nResponse: " << messageResponse << std::endl;
                return;
            }

            std::string originalSender;
            std::string originalSubject;
            bool foundSender = false;
            bool foundSubject = false;

            // Extract original sender and subject with logging
            if (emailData.contains("payload") &&
                emailData["payload"].is_object() &&
                emailData["payload"].contains("headers") &&
                emailData["payload"]["headers"].is_array()) {             

                for (const auto& header : emailData["payload"]["headers"]) {
                    if (!header.is_object() || !header.contains("name") || !header.contains("value")) {
                        continue;
                    }

                    std::string headerName = header["name"];
                    if (headerName == "From") {
                        originalSender = header["value"];
                        size_t start = originalSender.find('<');
                        size_t end = originalSender.find('>');
                        if (start != std::string::npos && end != std::string::npos) {
                            originalSender = originalSender.substr(start + 1, end - start - 1);
                        }
                        foundSender = true;                       
                    }
                    else if (headerName == "Subject") {
                        originalSubject = "Re: " + std::string(header["value"]);
                        foundSubject = true;                     
                    }
                }
            }

            if (!foundSender || !foundSubject) {
                std::cerr << "Failed to find sender or subject in email headers" << std::endl;
                return;
            }

            // Create response body based on operation type
            std::string responseBody;
           
            // Check socket status before receiving
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0 || error != 0) {
                std::cerr << "Socket error detected, reconnecting..." << std::endl;
                initializeSocket();  // Reconnect if socket is broken
            }

            if (std::ifstream file{ "screenshot.bmp" }) {
                responseBody = "Screenshot captured successfully and saved.";
                file.close();
                std::remove("screenshot.bmp");
                std::cout << "Processed screenshot" << std::endl;
            }
            else if (std::ifstream file{ "webcam.jpg" }) {
                responseBody = "Webcam capture completed successfully and saved.";
                file.close();
                std::remove("webcam.jpg");
                std::cout << "Processed webcam capture" << std::endl;
            }
            else if (std::ifstream file{ "recording.avi" }) {
                responseBody = "Webcam recording completed successfully and saved.";
                file.close();
                std::remove("recording.avi");
                std::cout << "Processed webcam recording" << std::endl;
            }
            else {
                std::cout << "Checking for server response..." << std::endl;
                std::vector<char> buffer(BUFFER_SIZE);

                // Set socket timeout
                DWORD timeout = 5000; // 5 seconds
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

                int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
                std::cout << "Received " << bytes_received << " bytes from server" << std::endl;

                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    responseBody = std::string(buffer.data(), bytes_received);
                    std::cout << "Server response: " << responseBody << std::endl;
                }
                else if (bytes_received == 0) {
                    std::cout << "Server closed connection, reconnecting..." << std::endl;
                    initializeSocket();
                    responseBody = "Command processed successfully.";
                }
                else {
                    int error = WSAGetLastError();
                    std::cout << "Socket error: " << error << std::endl;
                    responseBody = "Command processed successfully.";
                }
            }

            std::cout << "Sending email response..." << std::endl;
            if (!originalSender.empty() && !originalSubject.empty()) {
                sendEmailResponse(originalSender, originalSubject, responseBody);
                std::cout << "Email response sent successfully" << std::endl;
            }
        }
        catch (const json::exception& e) {
            std::cerr << "JSON error in sendEmailToOriginalSender: " << e.what() << std::endl;
            refreshAccessToken();
        }
        catch (const std::exception& e) {
            std::cerr << "Error sending response email: " << e.what() << std::endl;
            refreshAccessToken();
        }
        catch (...) {
            std::cerr << "Unknown error in sendEmailToOriginalSender" << std::endl;
        }
        std::cout << "Completed sendEmailToOriginalSender" << std::endl;
    }

    std::string readFileToBase64(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + filepath);
        }

        // Read file into buffer
        std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
        file.close();

        return base64_encode(std::string(buffer.begin(), buffer.end()));
    }

    std::string createMultipartMessage(const std::string& recipientEmail,
        const std::string& subject,
        const std::string& body,
        const std::string& imagePath) {
        // Generate a random boundary
        std::string boundary = "boundary" + std::to_string(std::rand());

        // Create the MIME message
        std::string message;
        message += "MIME-Version: 1.0\r\n";
        message += "From: me\r\n";
        message += "To: " + recipientEmail + "\r\n";
        message += "Subject: =?UTF-8?B?" + base64_encode(subject) + "?=\r\n";
        message += "Content-Type: multipart/mixed; boundary=" + boundary + "\r\n\r\n";

        // Text part
        message += "--" + boundary + "\r\n";
        message += "Content-Type: text/plain; charset=utf-8\r\n";
        message += "Content-Transfer-Encoding: base64\r\n\r\n";
        message += base64_encode(body) + "\r\n\r\n";

        // Image part
        message += "--" + boundary + "\r\n";
        message += "Content-Type: image/jpeg\r\n";
        message += "Content-Transfer-Encoding: base64\r\n";
        message += "Content-Disposition: attachment; filename=\"" +
            imagePath.substr(imagePath.find_last_of("/\\") + 1) + "\"\r\n\r\n";
        message += readFileToBase64(imagePath) + "\r\n\r\n";

        // End boundary
        message += "--" + boundary + "--\r\n";

        return message;
    }

    // Modify the sendEmailResponse function to handle attachments:
    void sendEmailResponse(const std::string& recipientEmail, const std::string& subject,
        const std::string& body, const std::string& imagePath = "") {
        try {
            std::cout << "Starting sendEmailResponse..." << std::endl;
            std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/send";

            // Create MIME message with proper line endings and encoding
            std::string rawMessage =
                "MIME-Version: 1.0\r\n"
                "From: me\r\n"
                "To: " + recipientEmail + "\r\n"
                "Subject: =?UTF-8?B?" + base64_encode(subject) + "?=\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "Content-Transfer-Encoding: base64\r\n"
                "\r\n" +
                base64_encode(body);

            // Base64 encode the entire raw message
            std::string encodedMessage = base64_encode(rawMessage);

            // Create the JSON payload
            json jsonPayload = {
                {"raw", encodedMessage}
            };

            std::string jsonString = jsonPayload.dump();

            // Send the email using CURL
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
            std::string response;

            if (curl) {
                struct curl_slist* headers = nullptr;
                headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");

                curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonString.c_str());
                curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);

                CURLcode res = curl_easy_perform(curl.get());

                if (res != CURLE_OK) {
                    throw std::runtime_error("CURL failed: " + std::string(curl_easy_strerror(res)));
                }

                long response_code;
                curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);

                if (response_code != 200) {
                    json error_response = json::parse(response);
                    if (error_response.contains("error") && error_response["error"].contains("message")) {
                        throw std::runtime_error("API error: " + std::string(error_response["error"]["message"]));
                    }
                    throw std::runtime_error("HTTP error: " + std::to_string(response_code));
                }

                curl_slist_free_all(headers);
            }
            else {
                throw std::runtime_error("Failed to initialize CURL");
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error in sendEmailResponse: " << e.what() << std::endl;
            // Attempt to refresh token and retry once
            try {
                refreshAccessToken();
                // Recursive call to retry with new token
                sendEmailResponse(recipientEmail, subject, body);
            }
            catch (const std::exception& retry_error) {
                throw std::runtime_error("Failed to send email after token refresh: " + std::string(retry_error.what()));
            }
        }
    }


public:
    GmailClient(const std::string& clientId, const std::string& clientSecret,
        const std::string& refreshToken)
        : clientId(clientId), clientSecret(clientSecret), refreshToken(refreshToken),
        isFirstRun(true) {
        lastCheckTime = std::chrono::system_clock::now();
        refreshAccessToken();
        initializeSocket();
    }

    /*void initializeSocket() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            throw std::runtime_error("Socket creation failed");
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            throw std::runtime_error("Connection failed");
        }
        else {
			std::cout << "Connected to server" << std::endl;
        }
    }*/
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
            DWORD timeout = 5000; // 5 seconds
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
        while (true) {
            try {
                std::string command = processNewEmails();
				handleServerCommand(command);
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
            catch (const std::runtime_error& e) {
                if (std::string(e.what()) == "Shutdown requested") {
                    break;
                }
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }

    ~GmailClient() {
        closesocket(sock);
        WSACleanup();
    }
};

int main() {
    try {
        GmailClient client(
            "612933153793-1teill41m3i7t1qkit497uh6q8nkkhmc.apps.googleusercontent.com",
            "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",
            "1//04sP2Z68i9I7NCgYIARAAGAQSNwF-L9IrSEFQu8CWlFVcdWXH-1quABuTo-wMgwguTzaD4B5Z5mmBqGbEoiWGtxSOxpDySLMbVs4"
        );

        client.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
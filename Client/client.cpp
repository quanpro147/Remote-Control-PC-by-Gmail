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



using json = nlohmann::json;
const int BUFFER_SIZE = 1024;

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

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
std::string base64_decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break; 
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
std::string getFirstLine(const std::string& str) {
    std::string result = str;
    while (!result.empty() && (std::isspace(result.back()) || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

class GmailClient {
private:
    static constexpr int PORT = 8080;
    static constexpr int BUFFER_SIZE = 1024;
    static constexpr int BUFFER_SIZE_PRO_MAX = 20480;
    bool isFirstRun;
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    std::string accessToken;
    std::string ownEmail;
    std::string senderEmail;
    std::set<std::string> processedIds;
    std::chrono::system_clock::time_point lastCheckTime;
    SOCKET sock;
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

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
                        if (command != "no command") std::cout << "New message processed, command: " << command << std::endl;                       
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
            std::string body, sender;

            // Extract sender and subject from headers
            for (const auto& header : emailData["payload"]["headers"]) {
                if (header["name"] == "From") {
                    sender = header["value"];
                    // Extract email address from "Name <email@domain.com>" format
                    size_t start = sender.find('<');
                    size_t end = sender.find('>');
                    if (start != std::string::npos && end != std::string::npos) {
                        sender = sender.substr(start + 1, end - start - 1);
                    }

                    // Skip if this email is from ourselves
                    if (sender == ownEmail) {
                        return "no command";
                    }
                    break;
                }
            }
            for (const auto& part : emailData["payload"]["parts"]) {
                if (part["mimeType"] == "text/plain") {
                    body = part["body"]["data"];
                    break; 
                }
            }
            body = base64_decode(body);
            body = getFirstLine(body);
           
            std::string command = sender + ": " + body;
            std::cout << "Processing new email: " << command << std::endl;
            send(sock, command.c_str(), command.length(), 0);

            std::cout << "Command sent to server: " << command << std::endl;
            std::cout << "---" << std::endl;
            this->senderEmail = sender;
            return command;
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing message " << messageId << ": " << e.what() << std::endl;
            return "invalid";
        }
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
        std::string request = "invalid";
        size_t colonPos = command.find(": ");
        if (colonPos != std::string::npos && colonPos + 2 < command.length()) {
            request = command.substr(colonPos + 2);
        }
        else if (command == "invalid" || command == "no command") {
            request = command;
        }
        std::cout << "Parsed request: " << request << std::endl;
        if (request == "exit") {
            throw std::runtime_error("Shutdown requested");
        }
        else if (request == "list" || request == "list services" || request == "screen capture" || request == "webcam capture" || request == "webcam record" ||
            request == "shutdown" || request == "getFile" || request == "getListApps" || request == "runApp" || request == "closeApp") {
            handleServerResponse(request);
        }
        else if (request == "no command" || request == "invalid") {
            return;
        }
        else {
            std::cout << "Invalid command: " << request << std::endl;
        }
    }

    void handleServerResponse(const std::string& command) {
        std::string filePath = "";
        if (command == "list" || command == "getListApps" || command == "list services") {           
        }
        else if (command == "screen capture") {
			filePath = "screenshot.bmp";
            receiveFile("screenshot.bmp");
        }
        else if (command == "webcam capture") {
			filePath = "webcam.jpg";
			receiveFile("webcam.jpg");
        }
        else if (command == "webcam record") {
            filePath = "webcam_record.avi";
            handleWebcamRecording();
        }
        else if (command == "getFile") {
			filePath = handleGetFile();
        }
        else if (command == "runApp"){
            filePath = "RunApp";
            handleRunApp();
        }
        else if (command == "closeApp") {
			handleCloseApp();
        }
        sendEmailToOriginalSender(filePath);
    }

    std::vector<char> receiveSeverReponse() {
        std::vector<char> buffer(BUFFER_SIZE_PRO_MAX);
        int bytes_received = recv(sock, buffer.data(), buffer.size() - 1, 0);
        std::cout << "Server response: " << std::string(buffer.data(), bytes_received) << std::endl;
        return buffer;
    }

    void receiveFile(const std::string& filename) {
        uint32_t fileSize = 0;
        int bytesReceived = recv(sock, reinterpret_cast<char*>(&fileSize), sizeof(uint32_t), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Failed to receive the file size. Error code: " << WSAGetLastError() << std::endl;
            return;
        }

        // Prepare a buffer to receive the file data
        std::vector<char> buffer(fileSize);
        size_t totalBytesReceived = 0;
        std::cout << "Starting to receive file..." << std::endl;

        while (totalBytesReceived < fileSize) {
            size_t remainingBytes = fileSize - totalBytesReceived;
            size_t chunkSize = (remainingBytes < BUFFER_SIZE) ? remainingBytes : BUFFER_SIZE;

            int bytes = recv(sock, buffer.data() + totalBytesReceived,
                static_cast<int>(chunkSize), 0);
            totalBytesReceived += bytes;
        }

        std::cout << "File received successfully." << std::endl;

        // Save the file to disk
        std::ofstream outFile(filename, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to open file for writing" << std::endl;
            return;
        }
        outFile.write(buffer.data(), fileSize);
        outFile.close();
        std::cout << "File saved as " << filename << std::endl;
    }

    std::string handleGetFile() {
        std::vector<char> buffer(BUFFER_SIZE);
        buffer = receiveSeverReponse();

        std::string filename;
		std::cout << "Enter the filename you want to get: ";
		getline(std::cin, filename);
		send(sock, filename.c_str(), filename.size() + 1, 0);
		receiveFile(filename);
		return filename;
    }

    void handleRunApp() {
        std::vector<char> buffer(BUFFER_SIZE_PRO_MAX);
        buffer = receiveSeverReponse();
		sendEmailResponse(this->senderEmail, std::string(buffer.data()));
        int appIndex;
        std::cout << "Enter the index of the app you want to run: ";
        std::cin >> appIndex;
        send(sock, std::to_string(appIndex).c_str(), std::to_string(appIndex).size() + 1, 0);
        char RESULT[100];
        int ByteReceived = recv(sock, RESULT, 100, 0);
        if (ByteReceived > 0) {
            std::cout << RESULT << std::endl;
        }
        else {
            std::cout << " ERROR" << std::endl;
        }
        
    }

    void handleCloseApp() {
        std::vector<char> buffer(BUFFER_SIZE);
        buffer = receiveSeverReponse();
        int appIndex;
        std::cout << "Enter the index of the app you want to close: ";
        std::cin >> appIndex;
        send(sock, std::to_string(appIndex).c_str(), std::to_string(appIndex).size() + 1, 0);
    }

    void handleWebcamRecording() {
        std::vector<char> buffer(BUFFER_SIZE);
        buffer = receiveSeverReponse();
        
        std::string duration;
        std::getline(std::cin, duration);
        int sent_bytes = send(sock, duration.c_str(), duration.length(), 0);
        std::cout << "Waiting for server to start recording...\n";

        buffer = receiveSeverReponse();
        if (std::string(buffer.data()) == "ok") {        
            receiveFile("webcam_record.avi");
        }
        else {
            throw std::runtime_error("Unexpected server response: " + std::string(buffer.data()));
        }
    }
    
    void sendEmailToOriginalSender(std::string filePath) {
        try {
            std::cout << "Starting sendEmailToOriginalSender..." << std::endl;
            if (processedIds.empty()) {
                std::cout << "No processed IDs found, returning..." << std::endl;
                return;
            }

            std::string lastMessageId = *processedIds.rbegin();          
            std::string messageUrl = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + lastMessageId;           
            std::string messageResponse = makeGetRequest(messageUrl);
           
            json emailData;
            try {
                emailData = json::parse(messageResponse);              
            }
            catch (const json::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << "\nResponse: " << messageResponse << std::endl;
                return;
            }
      
            std::string sender = this->senderEmail;
            std::string responseBody;
 
			if (filePath != "") {
                responseBody = "Request completed successfully.";
			}
            else{
                std::cout << "Checking for server response..." << std::endl;
                std::vector<char> buffer(BUFFER_SIZE);
                buffer = receiveSeverReponse();
                responseBody = std::string(buffer.data());
            }          
            std::cout << "Sending email response..." << std::endl;
            sendEmailResponse(sender, responseBody, filePath);
            std::cout << "Email response sent successfully" << std::endl;
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
    }

    std::string readFileToBase64(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + filepath);
        }
        std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});
        file.close();
        return base64_encode(std::string(buffer.begin(), buffer.end()));
    }
    std::string createMultipartMessage(const std::string& recipientEmail,
        const std::string& subject,
        const std::string& body,
        const std::string& filePath) {       
        std::string boundary = "boundary" + std::to_string(std::rand());

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

        // Video part
        message += "--" + boundary + "\r\n";

        // Detect video mime type based on extension
        std::string extension = filePath.substr(filePath.find_last_of(".") + 1);
        std::string mimeType = "video/mp4"; // default
        if (extension == "avi") mimeType = "video/x-msvideo";
        else if (extension == "mov") mimeType = "video/quicktime";
        else if (extension == "wmv") mimeType = "video/x-ms-wmv";

        message += "Content-Type: " + mimeType + "\r\n";
        message += "Content-Transfer-Encoding: base64\r\n";
        message += "Content-Disposition: attachment; filename=\"" +
            filePath.substr(filePath.find_last_of("/\\") + 1) + "\"\r\n\r\n";

        // Read and encode video in chunks
        const size_t CHUNK_SIZE = 1024 * 1024; // 1MB chunks
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open video file: " + filePath);
        }

        std::vector<char> buffer(CHUNK_SIZE);
        while (file) {
            file.read(buffer.data(), CHUNK_SIZE);
            std::streamsize bytesRead = file.gcount();
            if (bytesRead > 0) {
                message += base64_encode(std::string(buffer.data(), bytesRead));
                message += "\r\n";
            }
        }
        file.close();
        message += "--" + boundary + "--\r\n";
        return message;
    }
    void sendEmailResponse(const std::string& recipientEmail, const std::string& body, const std::string& filePath = "") {
        try {
            std::string subject = "";
            std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/send";
            std::string rawMessage;

            if (!filePath.empty() && std::ifstream(filePath).good()) {
                rawMessage = createMultipartMessage(recipientEmail, subject, body, filePath);
            }
            else {              
                rawMessage = "MIME-Version: 1.0\r\n"
                    "From: me\r\n"
                    "To: " + recipientEmail + "\r\n"
                    "Subject: =?UTF-8?B?" + base64_encode(subject) + "?=\r\n"
                    "Content-Type: text/plain; charset=utf-8\r\n"
                    "Content-Transfer-Encoding: base64\r\n\r\n" +
                    base64_encode(body);
            }

            std::string encodedMessage = base64_encode(rawMessage);
            json jsonPayload = {
                {"raw", encodedMessage}
            };

            std::string jsonString = jsonPayload.dump();
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

                // Increased timeout for large files
                curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 300L); // 5 minutes timeout
                // Set buffer size for faster uploads
                curl_easy_setopt(curl.get(), CURLOPT_BUFFERSIZE, 1024L * 1024L); // 1MB buffer
                // Enable progress callback for large uploads
                curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl.get(), CURLOPT_PROGRESSFUNCTION, ProgressCallback);

                CURLcode res = curl_easy_perform(curl.get());

                if (res != CURLE_OK) {
                    throw std::runtime_error("CURL failed: " + std::string(curl_easy_strerror(res)));
                }

                curl_slist_free_all(headers);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error in sendEmailResponse: " << e.what() << std::endl;
            refreshAccessToken();
            try {
                sendEmailResponse(recipientEmail, body, filePath);
            }
            catch (const std::exception& retry_error) {
                throw std::runtime_error("Failed to send email after token refresh: " +
                    std::string(retry_error.what()));
            }
        }
    }
    static int ProgressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
        if (ultotal > 0) {
            double progress = (ulnow / ultotal) * 100;
            std::cout << "Upload progress: " << static_cast<int>(progress) << "%" << std::endl;
        }
        return 0;
    }

public:
    GmailClient(const std::string& clientId, const std::string& clientSecret,
        const std::string& refreshToken, const std::string& ownEmail)
        : clientId(clientId), clientSecret(clientSecret), refreshToken(refreshToken),
        isFirstRun(true), ownEmail(ownEmail) {
        lastCheckTime = std::chrono::system_clock::now();
        refreshAccessToken();
        initializeSocket();
    }
    //IP may ao
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
            DWORD timeout = 300000;
            if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
                std::cerr << "Warning: Failed to set receive timeout" << std::endl;
            }

            sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1"/*"192.168.30.129"*/, &server_addr.sin_addr);

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
            "1//04sP2Z68i9I7NCgYIARAAGAQSNwF-L9IrSEFQu8CWlFVcdWXH-1quABuTo-wMgwguTzaD4B5Z5mmBqGbEoiWGtxSOxpDySLMbVs4",
            "quanphanpq147@gmail.com"
        );

        client.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
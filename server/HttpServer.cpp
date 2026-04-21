#include "HttpServer.h"
#include "../kernel/Globals.h"
#include "../kernel/Logger.h"
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

HttpServer::HttpServer(Kernel* k, int p) : kernel(k), port(p) {
    serverSocket = INVALID_SOCKET;
    running = false;
    dashboardPath = "dashboard";
}

void HttpServer::setDashboardPath(const string& path) {
    dashboardPath = path;
}

// =====================================================
//  HTTP REQUEST PARSING
// =====================================================

string HttpServer::parseMethod(const string& request) {
    size_t end = request.find(' ');
    if (end == string::npos) return "";
    return request.substr(0, end);
}

string HttpServer::parsePath(const string& request) {
    size_t start = request.find(' ');
    if (start == string::npos) return "/";
    start++;
    size_t end = request.find(' ', start);
    if (end == string::npos) return "/";
    return request.substr(start, end - start);
}

string HttpServer::parseBody(const string& request) {
    size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart == string::npos) return "";
    return request.substr(bodyStart + 4);
}

// =====================================================
//  HTTP RESPONSE BUILDERS
// =====================================================

void HttpServer::sendResponse(SOCKET sock, int code, const string& contentType, const string& body) {
    string statusText;
    if (code == 200) statusText = "OK";
    else if (code == 404) statusText = "Not Found";
    else if (code == 400) statusText = "Bad Request";
    else statusText = "Error";

    stringstream response;
    response << "HTTP/1.1 " << code << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    string resp = response.str();
    send(sock, resp.c_str(), (int)resp.size(), 0);
}

// =====================================================
//  STATIC FILE SERVING
// =====================================================

string HttpServer::getMimeType(const string& path) {
    if (path.find(".html") != string::npos) return "text/html; charset=utf-8";
    if (path.find(".css") != string::npos) return "text/css; charset=utf-8";
    if (path.find(".js") != string::npos) return "application/javascript; charset=utf-8";
    if (path.find(".json") != string::npos) return "application/json";
    if (path.find(".png") != string::npos) return "image/png";
    if (path.find(".ico") != string::npos) return "image/x-icon";
    return "text/plain";
}

string HttpServer::readFile(const string& path) {
    ifstream file(path, ios::binary);
    if (!file.is_open()) return "";
    stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool HttpServer::fileExists(const string& path) {
    ifstream f(path);
    return f.good();
}

// =====================================================
//  CLIENT REQUEST HANDLER — CORE ROUTING
// =====================================================

void HttpServer::handleClient(SOCKET clientSocket) {
    char buffer[8192] = {0};
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        closesocket(clientSocket);
        return;
    }

    string request(buffer, bytesRead);
    string method = parseMethod(request);
    string path = parsePath(request);

    // ===== CORS preflight =====
    if (method == "OPTIONS") {
        sendResponse(clientSocket, 200, "text/plain", "");
        closesocket(clientSocket);
        return;
    }

    // ===== API ROUTES =====

    // GET /api/state — Full kernel state as JSON
    if (method == "GET" && path == "/api/state") {
        string json = kernel->toJSON();
        sendResponse(clientSocket, 200, "application/json", json);
        closesocket(clientSocket);
        return;
    }

    // POST /api/command — Execute a command
    if (method == "POST" && path == "/api/command") {
        string body = parseBody(request);

        // Parse {"cmd": "..."} — simple extraction
        string cmd = "";
        size_t cmdStart = body.find("\"cmd\"");
        if (cmdStart != string::npos) {
            size_t valStart = body.find("\"", cmdStart + 5);
            if (valStart != string::npos) {
                valStart++;
                size_t valEnd = body.find("\"", valStart);
                if (valEnd != string::npos) {
                    cmd = body.substr(valStart, valEnd - valStart);
                }
            }
        }

        if (cmd.empty()) {
            sendResponse(clientSocket, 400, "application/json", "{\"ok\":false,\"error\":\"Missing cmd\"}");
        } else {
            string result = kernel->executeCommand(cmd);
            sendResponse(clientSocket, 200, "application/json", result);
        }
        closesocket(clientSocket);
        return;
    }

    // ===== STATIC FILE SERVING =====

    // Root → index.html
    if (path == "/" || path == "/index.html") {
        string content = readFile(dashboardPath + "/index.html");
        if (!content.empty()) {
            sendResponse(clientSocket, 200, "text/html; charset=utf-8", content);
        } else {
            sendResponse(clientSocket, 404, "text/plain", "Dashboard not found");
        }
        closesocket(clientSocket);
        return;
    }

    // Other static files (styles.css, app.js, etc.)
    // Security: prevent path traversal
    if (path.find("..") == string::npos) {
        string filePath = dashboardPath + path;
        if (fileExists(filePath)) {
            string content = readFile(filePath);
            sendResponse(clientSocket, 200, getMimeType(path), content);
            closesocket(clientSocket);
            return;
        }
    }

    // 404
    sendResponse(clientSocket, 404, "text/plain", "Not Found: " + path);
    closesocket(clientSocket);
}

// =====================================================
//  SERVER LIFECYCLE
// =====================================================

bool HttpServer::start() {
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "[HttpServer] Failed to create socket\n";
        return false;
    }

    // Allow address reuse
    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "[HttpServer] Failed to bind on port " << port << "\n";
        closesocket(serverSocket);
        return false;
    }

    // Listen
    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        cerr << "[HttpServer] Failed to listen\n";
        closesocket(serverSocket);
        return false;
    }

    running = true;
    serverThread.start(HttpServerThread, this);

    cout << "  Dashboard: http://localhost:" << port << "\n";
    sysLogger.log("[HttpServer]", "Listening on port " + to_string(port));
    return true;
}

void HttpServer::stop() {
    running = false;
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    if (serverThread.joinable()) {
        serverThread.join();
    }
}

void HttpServer::serverLoop() {
    while (running) {
        // Set timeout on accept to allow graceful shutdown
#if defined(_WIN32) || defined(_WIN64)
        DWORD timeout = 1000;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

        SOCKET client = accept(serverSocket, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        handleClient(client);
    }
}

// =====================================================
//  THREAD ENTRY POINT
// =====================================================

#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI HttpServerThread(LPVOID param) {
    HttpServer* server = (HttpServer*)param;
    server->serverLoop();
    return 0;
}
#else
void HttpServerThread(HttpServer* server) {
    server->serverLoop();
}
#endif

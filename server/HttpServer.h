#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../kernel/Kernel.h"
#include "../kernel/OS_Thread.h"
#include "../kernel/OS_Mutex.h"
#include <string>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

class HttpServer {
private:
    Kernel* kernel;
    int port;
    SOCKET serverSocket;
    std::atomic<bool> running;
    OS_Thread serverThread;
    std::string dashboardPath;     // path to dashboard/ directory

    // Request handling
    void handleClient(SOCKET clientSocket);
    std::string parseMethod(const std::string& request);
    std::string parsePath(const std::string& request);
    std::string parseBody(const std::string& request);

    // Response builders
    void sendResponse(SOCKET sock, int code, const std::string& contentType, const std::string& body);

    // Static file serving
    std::string getMimeType(const std::string& path);
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);

public:
    HttpServer(Kernel* k, int port = 8080);
    void setDashboardPath(const std::string& path);
    bool start();
    void stop();
    void serverLoop();
};

// Thread entry point
#if defined(_WIN32) || defined(_WIN64)
    DWORD WINAPI HttpServerThread(LPVOID param);
#else
    void HttpServerThread(HttpServer* server);
#endif

#endif

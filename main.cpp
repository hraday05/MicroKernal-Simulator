#include <iostream>
#include "kernel/Kernel.h"
#include "user/Shell.h"
#include "server/HttpServer.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
#endif

using namespace std;

int main() {
    // Initialize Winsock (Windows only)
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    int wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsResult != 0) {
        cerr << "[FATAL] WSAStartup failed: " << wsResult << "\n";
        return 1;
    }
#endif

    Kernel kernel;
    Shell shell(&kernel);
    HttpServer httpServer(&kernel, 8080);

    cout << "MicroKernel OS Started...\n";

    // NOTE: Scheduler NOT auto-started — processes stay in READY state
    // Use 'tick' command or '▶ Auto' button to run scheduling manually

    // Start HTTP server (serves dashboard + API)
    if (httpServer.start()) {
        cout << "  Backend API: http://localhost:8080/api/state\n";
    } else {
        cerr << "  [WARNING] HTTP server failed to start (port 8080 in use?)\n";
        cerr << "  CLI shell will still work.\n";
    }

    shell.run();                   // Main loop (blocks until "exit")

    httpServer.stop();             // Stop HTTP server
    kernel.stopScheduler();        // Stop CPU scheduler

    // Cleanup Winsock
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    return 0;
}

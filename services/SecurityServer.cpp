#include "SecurityServer.h"
#include "../kernel/Globals.h"
#include <iostream>

using namespace std;

void SecurityServer::handleMessage(Message msg) {
    if (msg.type == "request_capability") {
        // Issue tokens securely
        processCapabilities[msg.sender] = msg.data;
        OS_LockGuard lock(printMutex);
        cout << "[SecurityServer] Granted capability '" << msg.data 
             << "' to PID: " << msg.sender << "\n";
    }
}

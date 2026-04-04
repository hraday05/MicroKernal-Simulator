#include "IPC.h"
#include "../kernel/Globals.h"
#include <iostream>

using namespace std;

void MessageBus::sendMessage(Message msg) {
    // ----------------------------------------------------
    // SANDBOXING LAYER (Mechanism)
    // Here the Microkernel evaluates Capability Tokens!
    // ----------------------------------------------------
    if (msg.sender != 0 && msg.type == "file") {
        if (msg.capabilityToken != "CAP_FILE") {
            OS_LockGuard pLock(printMutex); 
            cout << "[Sandbox] DENIED: PID " << msg.sender 
                 << " attempted unauthorized file operation!\n";
            return;
        }
    }
    
    if (msg.sender != 0 && msg.type == "memory") {
        if (msg.capabilityToken != "CAP_MEM") {
            OS_LockGuard pLock(printMutex); 
            cout << "[Sandbox] DENIED: PID " << msg.sender 
                 << " attempted unauthorized memory operation!\n";
            return;
        }
    }

    OS_LockGuard lock(queueMutex);
    messageQueue.push(msg);
}

bool MessageBus::hasMessages() {
    OS_LockGuard lock(queueMutex);
    return !messageQueue.empty();
}

Message MessageBus::receiveMessage() {
    OS_LockGuard lock(queueMutex);
    Message msg = messageQueue.front();
    messageQueue.pop();
    return msg;
}

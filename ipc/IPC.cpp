#include "IPC.h"
#include "../kernel/Globals.h"
#include <iostream>

using namespace std;

void MessageBus::sendMessage(Message msg) {
    // No sandboxing here anymore — moved to Kernel for proper microkernel design
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

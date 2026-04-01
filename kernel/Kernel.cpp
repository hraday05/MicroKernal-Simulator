#include <iostream>
#include "Kernel.h"

using namespace std;

void Kernel::sendMessage(Message msg) {
    messageQueue.push(msg);
}

void Kernel::processMessages() {
    while (!messageQueue.empty()) {
        Message msg = messageQueue.front();
        messageQueue.pop();

        cout << "[Kernel] Routing message...\n";

        if (msg.data == "create_process" || msg.data == "list_process") {
          processServer.handleMessage(msg);
        }
    }
}
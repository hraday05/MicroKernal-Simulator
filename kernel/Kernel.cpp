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

        cout << "[Kernel] Routing message from "
             << msg.sender << " to "
             << msg.receiver
             << " | Type: " << msg.type
             << " | Data: " << msg.data << endl;
    }
}
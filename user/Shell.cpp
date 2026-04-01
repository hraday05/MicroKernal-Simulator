#include <iostream>
#include "Shell.h"

using namespace std;

Shell::Shell(Kernel* k) {
    kernel = k;
}

void Shell::run() {
    string command;

    while (true) {
        {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << ">> ";
        }
        cin >> command;

        if (command == "exit") break;

        Message msg;
        msg.sender = 1;      // Shell PID
        msg.receiver = 0;    // Kernel
        msg.type = "command";
        msg.data = command;

        kernel->sendMessage(msg);
        kernel->processMessages();
    }
}
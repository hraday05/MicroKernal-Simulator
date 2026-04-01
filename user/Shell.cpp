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
        kernel->stopScheduler();   // pause background noise
        getline(cin, command);
        kernel->startScheduler();  // resume. 

        if (command == "exit") break;

        Message msg;
        msg.sender = 1;      // Shell PID
        msg.receiver = 0;    // Kernel
        if (command.find("alloc") == 0) {
         msg.type = "memory";
         msg.data = command.substr(6); // number
        }
        else if (command == "free") {
          msg.type = "free";
          msg.data = "";
        }
        else {
          msg.type = "command";
          msg.data = command;
        }
        kernel->sendMessage(msg);
        kernel->processMessages();
    }
}
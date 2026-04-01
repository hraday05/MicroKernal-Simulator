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

         stringstream ss(command);
         string cmd;
         int amount, pid;

         ss >> cmd >> pid >> amount;

         if (ss.fail()) {
         cout << "Usage: alloc <amount> <pid>\n";
         continue;
        }

         msg.data = to_string(amount);
         msg.sender = pid;   // ✅ IMPORTANT: assign to target process
        }
        
        else if (command.find("free") == 0) {
          msg.type = "free";

          stringstream ss(command);
          string cmd;
          int amount, pid;
      
          ss >> cmd >> amount >> pid;
      
          msg.data = to_string(amount);
          msg.sender = pid;
        }
        else {
          msg.type = "command";
          msg.data = command;
        }
        kernel->sendMessage(msg);
        kernel->processMessages();
    }
}
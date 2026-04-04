#include <iostream>
#include <sstream>
#include "../kernel/OS_Mutex.h"
#include "Shell.h"
#include "../kernel/Globals.h"

using namespace std;

Shell::Shell(Kernel* k) {
    kernel = k;
}

void Shell::run() {
    string command;

    while (true) {
        {
        OS_LockGuard lock(printMutex);
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
         msg.capabilityToken = "CAP_MEM";

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
        else if (command.find("create_file") == 0 ||
                 command.find("read_file") == 0 ||
                 command.find("delete_file") == 0 ||
                 command.find("write_file") == 0) {
            msg.type = "file";
            msg.data = command;
            msg.capabilityToken = "CAP_FILE"; // Prove we have the token
        }
        else if (command.find("kill_service") == 0) {
            msg.type = "kill_service";
            msg.data = "FileService"; 
        }
        else {
          msg.type = "command";
          msg.data = command;
        }
        kernel->sendMessage(msg);
        kernel->processMessages();
    }
}

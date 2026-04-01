#include <iostream>
#include "ProcessServer.h"

using namespace std;

ProcessServer::ProcessServer() {
    nextPID = 100; // start from 100
}

void ProcessServer::handleMessage(Message msg) {

    if (msg.type == "command") {

        if (msg.data == "create_process") {
            Process p;
            p.pid = nextPID++;
            p.name = "Process_" + to_string(p.pid);

            processes.push_back(p);

            cout << "[ProcessServer] Created Process PID: "
                 << p.pid << endl;
        }

        else if (msg.data == "list_process") {
            cout << "[ProcessServer] Active Processes:\n";

            for (auto &p : processes) {
                cout << "PID: " << p.pid
                     << " Name: " << p.name << endl;
            }
        }

        else {
            cout << "[ProcessServer] Unknown command\n";
        }
    }
}
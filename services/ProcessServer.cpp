#include <iostream>
#include "ProcessServer.h"
#include "../kernel/Globals.h"

using namespace std;

ProcessServer::ProcessServer() {
    nextPID = 100; // start from 100
}

Process ProcessServer::getLastProcess() {
    return processes.back();
}

void ProcessServer::handleMessage(Message msg) {

    if (msg.type == "command") {

        if (msg.data == "create_process") {
            Process p;
            p.pid = nextPID++;
            p.name = "Process_" + to_string(p.pid);
            p.burstTime = 5; // default CPU time

            processes.push_back(p);
            {
            std::lock_guard<std::mutex> lock(printMutex);
            cout << "[ProcessServer] Created Process PID: "
                 << p.pid << " (Burst: " << p.burstTime << ")\n";
            }
        }

        else if (msg.data == "list_process") {
            {
            std::lock_guard<std::mutex> lock(printMutex);
            cout << "[ProcessServer] Active Processes:\n";
            }
            for (auto &p : processes) {
                {
                std::lock_guard<std::mutex> lock(printMutex);
                cout << "PID: " << p.pid
                     << " Name: " << p.name << endl;
            }}
        }

        else {
            std::lock_guard<std::mutex> lock(printMutex);
            cout << "[ProcessServer] Unknown command\n";
        }
    }
}
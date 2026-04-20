#include <iostream>
#include <sstream>
#include "ProcessServer.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/Globals.h"

using namespace std;

static const int DEFAULT_BURST = 30;

ProcessServer::ProcessServer() {
    nextPID = 100;
}

PCB ProcessServer::getLastProcess() {
    return processes.back();
}

bool ProcessServer::processExists(int pid) {
    for (auto &p : processes) {
        if (p.pid == pid) return true;
    }
    return false;
}

void ProcessServer::handleMessage(Message msg) {

    if (msg.type == "command") {

        if (msg.data == "create_process") {
            PCB p;
            p.pid = nextPID++;
            p.name = "Process_" + to_string(p.pid);

            // Use burst from msg.receiver if provided, otherwise use default
            int burst = msg.receiver;
            if (burst <= 0) burst = DEFAULT_BURST;
            p.burstTime = burst;
            p.remainingTime = burst;   // remaining starts equal to burst
            p.state = "READY";

            processes.push_back(p);
            {
                OS_LockGuard lock(printMutex);
                cout << "[ProcessServer] Created Process PID: "
                     << p.pid << " (Burst: " << p.burstTime << ")\n";
            }
        }

        else if (msg.data == "list_process") {
            OS_LockGuard lock(printMutex);
            cout << "\n[ProcessServer] Active Processes:\n";
            if (processes.empty()) {
                cout << "  No active processes.\n\n";
            } else {
                for (auto &p : processes) {
                    cout << "  PID: " << p.pid
                         << "  Name: " << p.name
                         << "  Burst: " << p.burstTime
                         << "  Remaining: " << p.remainingTime
                         << "  State: " << p.state << "\n";
                }
                cout << "\n";
            }
        }

        else {
            OS_LockGuard lock(printMutex);
            cout << "[ProcessServer] Unknown command\n";
        }
    }
}

void ProcessServer::removeProcess(int pid) {
    for (auto it = processes.begin(); it != processes.end(); ++it) {
        if (it->pid == pid) {
            processes.erase(it);

            OS_LockGuard lock(printMutex);
            cout << "[ProcessServer] Removed PID: " << pid << endl;
            return;
        }
    }
}

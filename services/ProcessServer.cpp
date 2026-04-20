#include <iostream>
#include "ProcessServer.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/Globals.h"

using namespace std;

ProcessServer::ProcessServer() {
    nextPID = 100; // start from 100
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
            p.burstTime = 30; // Increased from 5 to 30 so user has time to interact
            p.state = "READY";

            processes.push_back(p);
            {
            OS_LockGuard lock(printMutex);
            cout << "[ProcessServer] Created Process PID: "
                 << p.pid << " (Burst: " << p.burstTime << ")\n";
            }
        }

        else if (msg.data == "list_process") {
            {
            OS_LockGuard lock(printMutex);
            cout << "[ProcessServer] Active Processes:\n";
            }
            for (auto &p : processes) {
                {
                OS_LockGuard lock(printMutex);
                cout << "PID: " << p.pid
                     << " Name: " << p.name << endl;
            }}
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

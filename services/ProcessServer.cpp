#include <iostream>
#include <sstream>
#include <iomanip>
#include "ProcessServer.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/Globals.h"
#include "../kernel/Logger.h"

using namespace std;

static const int DEFAULT_BURST = 30;
static const int DEFAULT_PRIORITY = 5;

ProcessServer::ProcessServer() {
    nextPID = 100;
}

PCB ProcessServer::getLastProcess() {
    if (processes.empty()) {
        PCB empty;
        empty.pid = -1;
        empty.name = "NONE";
        empty.burstTime = 0;
        empty.remainingTime = 0;
        empty.priority = 0;
        empty.state = "DEAD";
        return empty;
    }
    return processes.back();
}

bool ProcessServer::processExists(int pid) {
    for (auto &p : processes) {
        if (p.pid == pid) return true;
    }
    return false;
}

PCB* ProcessServer::findProcess(int pid) {
    for (auto &p : processes) {
        if (p.pid == pid) return &p;
    }
    return nullptr;
}

bool ProcessServer::setProcessState(int pid, const string& state) {
    PCB* p = findProcess(pid);
    if (p) {
        p->state = state;
        return true;
    }
    return false;
}

void ProcessServer::handleMessage(Message msg) {
    if (msg.type == "command") {
        if (msg.data == "create_process") {
            PCB p;
            p.pid = nextPID++;
            p.name = "Process_" + to_string(p.pid);

            int burst = msg.receiver;
            if (burst <= 0) burst = DEFAULT_BURST;
            p.burstTime = burst;
            p.remainingTime = burst;

            int prio = DEFAULT_PRIORITY;
            try { prio = stoi(msg.capabilityToken); } catch(...) {}
            if (prio < 1) prio = 1;
            if (prio > 10) prio = 10;
            p.priority = prio;

            p.state = "READY";
            processes.push_back(p);

            {
                OS_LockGuard lock(printMutex);
                cout << "[ProcessServer] Created Process PID: "
                     << p.pid << " (Burst: " << p.burstTime
                     << ", Priority: " << p.priority << ")\n";
            }
            sysLogger.log("[ProcessServer]", "Created PID " + to_string(p.pid) +
                          " burst=" + to_string(p.burstTime) + " pri=" + to_string(p.priority));
        }
        else if (msg.data == "list_process") {
            OS_LockGuard lock(printMutex);
            cout << "\n[ProcessServer] Active Processes:\n";
            if (processes.empty()) {
                cout << "  No active processes.\n\n";
            } else {
                cout << "  +-------+--------------------+--------+-----------+----------+---------+\n";
                cout << "  |  PID  |       Name         | Burst  | Remaining | Priority |  State  |\n";
                cout << "  +-------+--------------------+--------+-----------+----------+---------+\n";
                for (auto &p : processes) {
                    cout << "  | " << setw(5) << p.pid << " | "
                         << setw(18) << left << p.name << right << " | "
                         << setw(6) << p.burstTime << " | "
                         << setw(9) << p.remainingTime << " | "
                         << setw(8) << p.priority << " | "
                         << setw(7) << p.state << " |\n";
                }
                cout << "  +-------+--------------------+--------+-----------+----------+---------+\n\n";
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
            sysLogger.log("[ProcessServer]", "Removed PID " + to_string(pid));
            return;
        }
    }
}

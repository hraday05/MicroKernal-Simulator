#include "SchedulerService.h"
#include "../kernel/Globals.h"
#include <iostream>

using namespace std;

SchedulerService::SchedulerService(ProcessServer* ps) : processServer(ps) {
    timeQuantum = 2; // fixed time slice
    hasCurrent = false;
}

void SchedulerService::addProcess(PCB p) {
    readyQueue.push(p);
}

vector<PCB> SchedulerService::getProcessSnapshot() {
    vector<PCB> snapshot;

    // Include the currently running process first
    if (hasCurrent) {
        snapshot.push_back(currentRunning);
    }

    // Walk the ready queue (copy it to iterate)
    queue<PCB> tempQueue = readyQueue;
    while (!tempQueue.empty()) {
        snapshot.push_back(tempQueue.front());
        tempQueue.pop();
    }

    return snapshot;
}

void SchedulerService::handleMessage(Message msg) {
    if (msg.type == "interrupt" && msg.data == "timer") {
        if (readyQueue.empty()) {
            hasCurrent = false;
            return;
        }

        PCB current = readyQueue.front();
        readyQueue.pop();

        int prevPID = hasCurrent ? currentRunning.pid : -1;
        current.state = "RUNNING";
        currentRunning = current;
        hasCurrent = true;

        // Silent context switching — use 'ps' to see live status
        // Only important events (completion) are printed to keep console clean

        current.burstTime -= timeQuantum;

        if (current.burstTime > 0) {
            current.state = "READY";
            readyQueue.push(current); // re-add for round robin
        } else {
            current.state = "DEAD";
            hasCurrent = false;
            {
                OS_LockGuard lock(printMutex);
                cout << "[RR] Process PID " << current.pid << " completed execution\n";
            }
            processServer->removeProcess(current.pid);
        }
    }
}

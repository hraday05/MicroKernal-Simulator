#include "SchedulerService.h"
#include "../kernel/Globals.h"
#include <iostream>

using namespace std;

SchedulerService::SchedulerService(ProcessServer* ps) : processServer(ps) {
    timeQuantum = 2; // fixed time slice
}

void SchedulerService::addProcess(PCB p) {
    readyQueue.push(p);
}

void SchedulerService::handleMessage(Message msg) {
    // We act purely on Hardware Timer Interrupts passed by the Microkernel Mechanics!
    if (msg.type == "interrupt" && msg.data == "timer") {
        if (readyQueue.empty()) return;

        PCB current = readyQueue.front();
        readyQueue.pop();
        current.state = "RUNNING";

        {
            static int lastPID = -1;
            if (current.pid != lastPID) {
                OS_LockGuard lock(printMutex);
                cout << "[SchedulerService] Running PID: " << current.pid << endl;
                lastPID = current.pid;
            }
        }

        current.burstTime -= timeQuantum;

        if (current.burstTime > 0) {
            current.state = "READY";
            readyQueue.push(current); // re-add
        } else {
            current.state = "DEAD";
            OS_LockGuard lock(printMutex);
            cout << "[SchedulerService] Process " << current.pid << " finished\n";
            processServer->removeProcess(current.pid);
            
            // In a pure microkernel, we would send an IPC message to MemoryService here.
            // For now, we will assume ProcessServer handles cleanup.
        }
    }
}

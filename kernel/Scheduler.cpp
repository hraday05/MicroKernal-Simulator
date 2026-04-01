#include <iostream>
#include "Scheduler.h"
#include "Globals.h"

using namespace std;

Scheduler::Scheduler() {
    timeQuantum = 2; // fixed time slice
}

void Scheduler::addProcess(Process p) {
    readyQueue.push(p);
}

void Scheduler::run() {

    if (readyQueue.empty()) {
    return;
    }

    Process current = readyQueue.front();
    readyQueue.pop();

    {
    static int lastPID = -1;

     if (current.pid != lastPID) {
       std::lock_guard<std::mutex> lock(printMutex);
       cout << "[Scheduler] Running PID: "
         << current.pid << endl;
         lastPID = current.pid;
     }
    }

    current.burstTime -= timeQuantum;

    if (current.burstTime > 0) {
        readyQueue.push(current); // re-add
    } else {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[Scheduler] Process "
             << current.pid << " finished\n";
    }
}
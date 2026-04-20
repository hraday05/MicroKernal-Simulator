#include "SchedulerService.h"
#include "../kernel/Globals.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

SchedulerService::SchedulerService(ProcessServer* ps) : processServer(ps) {
    timeQuantum = 2;
    hasCurrent = false;
    currentTime = 0;
    totalContextSwitches = 0;
    totalCompletions = 0;
}

void SchedulerService::addProcess(PCB p) {
    readyQueue.push(p);
}

vector<PCB> SchedulerService::getProcessSnapshot() {
    vector<PCB> snapshot;

    if (hasCurrent) {
        snapshot.push_back(currentRunning);
    }

    queue<PCB> tempQueue = readyQueue;
    while (!tempQueue.empty()) {
        snapshot.push_back(tempQueue.front());
        tempQueue.pop();
    }

    return snapshot;
}

void SchedulerService::handleMessage(Message msg) {
    if (msg.type == "interrupt" && msg.data == "timer") {

        // Nothing to schedule
        if (readyQueue.empty() && !hasCurrent) {
            return;
        }

        // If no process is currently running, pick one from the queue
        if (!hasCurrent && !readyQueue.empty()) {
            currentRunning = readyQueue.front();
            readyQueue.pop();
            currentRunning.state = "RUNNING";
            hasCurrent = true;
            totalContextSwitches++;
        }

        if (!hasCurrent) return;

        // Calculate actual run time: min(quantum, remaining)
        int runTime = (timeQuantum < currentRunning.remainingTime)
                       ? timeQuantum : currentRunning.remainingTime;

        // ===== LOG THIS EXECUTION SLOT FOR GANTT CHART =====
        ScheduleEntry entry;
        entry.pid = currentRunning.pid;
        entry.startTime = currentTime;
        entry.endTime = currentTime + runTime;
        executionLog.push_back(entry);

        currentTime += runTime;
        currentRunning.remainingTime -= runTime;

        if (currentRunning.remainingTime <= 0) {
            // ===== PROCESS COMPLETED =====
            currentRunning.state = "DEAD";
            totalCompletions++;

            {
                OS_LockGuard lock(printMutex);
                cout << "[RR] Process PID " << currentRunning.pid
                     << " completed execution (total CPU time: "
                     << currentRunning.burstTime << ")\n";
            }

            // Notify MemoryService to free all memory for this PID
            Message freeMsg;
            freeMsg.sender = currentRunning.pid;
            freeMsg.receiver = 0;
            freeMsg.type = "process_dead";
            freeMsg.data = to_string(currentRunning.pid);
            if (bus) bus->sendMessage(freeMsg);

            processServer->removeProcess(currentRunning.pid);
            hasCurrent = false;

            // Immediately pick next process if available
            if (!readyQueue.empty()) {
                currentRunning = readyQueue.front();
                readyQueue.pop();
                currentRunning.state = "RUNNING";
                hasCurrent = true;
                totalContextSwitches++;
            }

        } else {
            // ===== TIME QUANTUM EXPIRED — PREEMPT =====
            currentRunning.state = "READY";
            readyQueue.push(currentRunning);

            // Pick next process from front of queue
            currentRunning = readyQueue.front();
            readyQueue.pop();
            currentRunning.state = "RUNNING";
            hasCurrent = true;
            totalContextSwitches++;
        }
    }
}

// =====================================================
//  GANTT CHART VISUALIZATION
// =====================================================

void SchedulerService::printGanttChart() {
    OS_LockGuard lock(printMutex);

    if (executionLog.empty()) {
        cout << "\n  No scheduling data available yet.\n";
        cout << "  Create some processes first with 'create_process [burst]'!\n\n";
        return;
    }

    cout << "\n";
    cout << "  ================================================\n";
    cout << "   GANTT CHART - Round Robin (Quantum = " << timeQuantum << ")\n";
    cout << "  ================================================\n\n";

    // --- Top border ---
    cout << "  +";
    for (size_t i = 0; i < executionLog.size(); i++) {
        cout << "------+";
    }
    cout << "\n";

    // --- Process IDs ---
    cout << "  |";
    for (auto& e : executionLog) {
        // Format: " P100 " (6 chars)
        stringstream ss;
        ss << "P" << e.pid;
        string label = ss.str();

        // Center the label in 6 chars
        int totalPad = 6 - (int)label.length();
        int leftPad = totalPad / 2;
        int rightPad = totalPad - leftPad;

        cout << string(leftPad, ' ') << label << string(rightPad, ' ') << "|";
    }
    cout << "\n";

    // --- Bottom border ---
    cout << "  +";
    for (size_t i = 0; i < executionLog.size(); i++) {
        cout << "------+";
    }
    cout << "\n";

    // --- Time labels (aligned with cell borders) ---
    cout << "  ";
    string firstLabel = to_string(executionLog[0].startTime);
    cout << firstLabel;

    for (size_t i = 0; i < executionLog.size(); i++) {
        string endLabel = to_string(executionLog[i].endTime);
        int padding = 7 - (int)endLabel.length();
        if (padding < 1) padding = 1;
        cout << string(padding, ' ') << endLabel;
    }
    cout << "\n\n";
}

// =====================================================
//  DETAILED EXECUTION LOG
// =====================================================

void SchedulerService::printDetailedLog() {
    OS_LockGuard lock(printMutex);

    if (executionLog.empty()) {
        cout << "\n  No execution log available.\n\n";
        return;
    }

    cout << "  Detailed Execution Log:\n";
    cout << "  +---------+-------+-------------------+\n";
    cout << "  |  Time   |  PID  |      Action       |\n";
    cout << "  +---------+-------+-------------------+\n";

    for (auto& e : executionLog) {
        cout << "  | t=" << setw(2) << e.startTime << "-"
             << setw(2) << left << e.endTime << right
             << " | " << setw(5) << e.pid
             << " | RUNNING           |\n";
    }
    cout << "  +---------+-------+-------------------+\n\n";
}

// =====================================================
//  SCHEDULER STATISTICS
// =====================================================

void SchedulerService::printStats() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Scheduler Statistics ==========\n";
    cout << "  Algorithm:          Round Robin\n";
    cout << "  Time Quantum:       " << timeQuantum << "\n";
    cout << "  Current CPU Time:   " << currentTime << "\n";
    cout << "  Context Switches:   " << totalContextSwitches << "\n";
    cout << "  Processes Completed:" << totalCompletions << "\n";

    // Count currently scheduled processes
    int inQueue = (int)readyQueue.size() + (hasCurrent ? 1 : 0);
    cout << "  Processes in Queue: " << inQueue << "\n";

    if (hasCurrent) {
        cout << "  Currently Running:  PID " << currentRunning.pid
             << " (remaining: " << currentRunning.remainingTime << ")\n";
    } else {
        cout << "  Currently Running:  IDLE\n";
    }

    cout << "  Execution Slots:    " << executionLog.size() << "\n";
    cout << "  ==========================================\n\n";
}

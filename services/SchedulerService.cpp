#include "SchedulerService.h"
#include "../kernel/Globals.h"
#include "../kernel/Logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

SchedulerService::SchedulerService(ProcessServer* ps) : processServer(ps) {
    timeQuantum = 5;
    lastEffectiveQuantum = 5;
    hasCurrent = false;
    currentTime = 0;
    totalContextSwitches = 0;
    totalSkippedSwitches = 0;
    totalCompletions = 0;
    algorithm = SchedulerAlgorithm::ROUND_ROBIN;
}

void SchedulerService::addProcess(PCB p) {
    readyList.push_back(p);
}

// =====================================================
//  PROCESS SIGNALS
// =====================================================

bool SchedulerService::suspendProcess(int pid) {
    // Check if it's the currently running process
    if (hasCurrent && currentRunning.pid == pid) {
        currentRunning.state = "BLOCKED";
        blockedList.push_back(currentRunning);
        hasCurrent = false;

        OS_LockGuard lock(printMutex);
        cout << "[Scheduler] SUSPENDED PID " << pid << " (was RUNNING → BLOCKED)\n";
        sysLogger.log("[Scheduler]", "SUSPENDED PID " + to_string(pid));

        // Pick next process
        if (!readyList.empty()) {
            int idx = pickNextIndex();
            currentRunning = readyList[idx];
            readyList.erase(readyList.begin() + idx);
            currentRunning.state = "RUNNING";
            hasCurrent = true;
            totalContextSwitches++;
        }
        return true;
    }

    // Check ready list
    for (auto it = readyList.begin(); it != readyList.end(); ++it) {
        if (it->pid == pid) {
            it->state = "BLOCKED";
            blockedList.push_back(*it);
            readyList.erase(it);

            OS_LockGuard lock(printMutex);
            cout << "[Scheduler] SUSPENDED PID " << pid << " (READY → BLOCKED)\n";
            sysLogger.log("[Scheduler]", "SUSPENDED PID " + to_string(pid));
            return true;
        }
    }

    OS_LockGuard lock(printMutex);
    cout << "[Scheduler] PID " << pid << " not found in scheduler.\n";
    return false;
}

bool SchedulerService::resumeProcess(int pid) {
    for (auto it = blockedList.begin(); it != blockedList.end(); ++it) {
        if (it->pid == pid) {
            it->state = "READY";
            readyList.push_back(*it);
            blockedList.erase(it);

            OS_LockGuard lock(printMutex);
            cout << "[Scheduler] RESUMED PID " << pid << " (BLOCKED → READY)\n";
            sysLogger.log("[Scheduler]", "RESUMED PID " + to_string(pid));
            return true;
        }
    }

    OS_LockGuard lock(printMutex);
    cout << "[Scheduler] PID " << pid << " not found in blocked list.\n";
    return false;
}

bool SchedulerService::killProcess(int pid) {
    // Kill currently running process
    if (hasCurrent && currentRunning.pid == pid) {
        hasCurrent = false;
        totalCompletions++;

        {
            OS_LockGuard lock(printMutex);
            cout << "[Scheduler] KILLED PID " << pid << " (was RUNNING)\n";
        }
        sysLogger.log("[Scheduler]", "KILLED PID " + to_string(pid));

        Message freeMsg;
        freeMsg.sender = pid;
        freeMsg.receiver = 0;
        freeMsg.type = "process_dead";
        freeMsg.data = to_string(pid);
        if (bus) bus->sendMessage(freeMsg);

        processServer->removeProcess(pid);

        if (!readyList.empty()) {
            int idx = pickNextIndex();
            currentRunning = readyList[idx];
            readyList.erase(readyList.begin() + idx);
            currentRunning.state = "RUNNING";
            hasCurrent = true;
            totalContextSwitches++;
        }
        return true;
    }

    // Kill from ready list
    for (auto it = readyList.begin(); it != readyList.end(); ++it) {
        if (it->pid == pid) {
            readyList.erase(it);

            {
                OS_LockGuard lock(printMutex);
                cout << "[Scheduler] KILLED PID " << pid << " (was READY)\n";
            }
            sysLogger.log("[Scheduler]", "KILLED PID " + to_string(pid));

            Message freeMsg;
            freeMsg.sender = pid;
            freeMsg.type = "process_dead";
            if (bus) bus->sendMessage(freeMsg);

            processServer->removeProcess(pid);
            return true;
        }
    }

    // Kill from blocked list
    for (auto it = blockedList.begin(); it != blockedList.end(); ++it) {
        if (it->pid == pid) {
            blockedList.erase(it);

            {
                OS_LockGuard lock(printMutex);
                cout << "[Scheduler] KILLED PID " << pid << " (was BLOCKED)\n";
            }
            sysLogger.log("[Scheduler]", "KILLED PID " + to_string(pid));

            Message freeMsg;
            freeMsg.sender = pid;
            freeMsg.type = "process_dead";
            if (bus) bus->sendMessage(freeMsg);

            processServer->removeProcess(pid);
            return true;
        }
    }

    OS_LockGuard lock(printMutex);
    cout << "[Scheduler] PID " << pid << " not found.\n";
    return false;
}

// =====================================================
//  ALGORITHM-BASED NEXT PROCESS SELECTION
// =====================================================

int SchedulerService::pickNextIndex() {
    if (readyList.empty()) return -1;

    switch (algorithm) {
        case SchedulerAlgorithm::ROUND_ROBIN:
            return 0;

        case SchedulerAlgorithm::PRIORITY: {
            int bestIdx = 0;
            for (int i = 1; i < (int)readyList.size(); i++) {
                if (readyList[i].priority < readyList[bestIdx].priority)
                    bestIdx = i;
            }
            return bestIdx;
        }

        case SchedulerAlgorithm::SJF: {
            int bestIdx = 0;
            for (int i = 1; i < (int)readyList.size(); i++) {
                if (readyList[i].remainingTime < readyList[bestIdx].remainingTime)
                    bestIdx = i;
            }
            return bestIdx;
        }
    }
    return 0;
}

vector<PCB> SchedulerService::getProcessSnapshot() {
    vector<PCB> snapshot;

    if (hasCurrent) {
        snapshot.push_back(currentRunning);
    }

    for (auto& p : readyList) {
        snapshot.push_back(p);
    }

    // Include blocked processes in snapshot
    for (auto& p : blockedList) {
        snapshot.push_back(p);
    }

    // Include recently completed (DEAD) processes
    for (auto& p : completedList) {
        snapshot.push_back(p);
    }

    return snapshot;
}

// =====================================================
//  SCHEDULING
// =====================================================

void SchedulerService::handleMessage(Message msg) {
    if (msg.type == "interrupt" && msg.data == "timer") {

        if (readyList.empty() && !hasCurrent) return;

        if (!hasCurrent && !readyList.empty()) {
            int idx = pickNextIndex();
            currentRunning = readyList[idx];
            readyList.erase(readyList.begin() + idx);
            currentRunning.state = "RUNNING";
            hasCurrent = true;
            totalContextSwitches++;
        }

        if (!hasCurrent) return;

        // Adaptive time quantum
        int numActive = (int)readyList.size() + 1;
        int effectiveQuantum = timeQuantum;

        if (algorithm == SchedulerAlgorithm::SJF || algorithm == SchedulerAlgorithm::PRIORITY) {
            // Non-preemptive: run the selected process to completion
            effectiveQuantum = currentRunning.remainingTime;
        }
        else {
            // Round Robin: adaptive quantum based on load
            if (numActive == 1) {
                effectiveQuantum = timeQuantum * 10;   // alone: 10x
            } else if (numActive == 2) {
                int avgRemaining = currentRunning.remainingTime;
                if (!readyList.empty()) avgRemaining = (avgRemaining + readyList[0].remainingTime) / 2;
                if (avgRemaining > 100) {
                    effectiveQuantum = timeQuantum * 3;   // 3x — high burst, reduce switches
                } else if (avgRemaining > 40) {
                    effectiveQuantum = timeQuantum * 2;   // 2x — moderate burst
                }
            } else if (numActive <= 4) {
                effectiveQuantum = timeQuantum * 2;
            }
        }

        int runTime = (effectiveQuantum < currentRunning.remainingTime)
                       ? effectiveQuantum : currentRunning.remainingTime;
        lastEffectiveQuantum = effectiveQuantum;  // Track for dashboard

        // Log for Gantt chart
        ScheduleEntry entry;
        entry.pid = currentRunning.pid;
        entry.startTime = currentTime;
        entry.endTime = currentTime + runTime;
        executionLog.push_back(entry);

        currentTime += runTime;
        currentRunning.remainingTime -= runTime;

        if (currentRunning.remainingTime <= 0) {
            currentRunning.state = "DEAD";
            totalCompletions++;

            {
                OS_LockGuard lock(printMutex);
                cout << "[" << getAlgorithmName() << "] Process PID " << currentRunning.pid
                     << " completed execution (CPU time: "
                     << currentRunning.burstTime << ")\n";
            }
            sysLogger.log("[Scheduler]", "PID " + to_string(currentRunning.pid) + " completed");

            Message freeMsg;
            freeMsg.sender = currentRunning.pid;
            freeMsg.receiver = 0;
            freeMsg.type = "process_dead";
            freeMsg.data = to_string(currentRunning.pid);
            if (bus) bus->sendMessage(freeMsg);

            processServer->removeProcess(currentRunning.pid);
            completedList.push_back(currentRunning);  // Keep for dashboard
            if (completedList.size() > 6) completedList.erase(completedList.begin());
            hasCurrent = false;

            if (!readyList.empty()) {
                int idx = pickNextIndex();
                currentRunning = readyList[idx];
                readyList.erase(readyList.begin() + idx);
                currentRunning.state = "RUNNING";
                hasCurrent = true;
                totalContextSwitches++;
            }

        } else {
            if (readyList.empty()) {
                totalSkippedSwitches++;
            } else {
                currentRunning.state = "READY";
                readyList.push_back(currentRunning);

                int idx = pickNextIndex();
                currentRunning = readyList[idx];
                readyList.erase(readyList.begin() + idx);
                currentRunning.state = "RUNNING";
                hasCurrent = true;
                totalContextSwitches++;
            }
        }
    }
}

// =====================================================
//  ALGORITHM MANAGEMENT
// =====================================================

void SchedulerService::setAlgorithm(SchedulerAlgorithm algo) {
    algorithm = algo;
    OS_LockGuard lock(printMutex);
    cout << "[Scheduler] Algorithm changed to: " << getAlgorithmName() << "\n";
    sysLogger.log("[Scheduler]", "Algorithm changed to " + getAlgorithmName());
}

string SchedulerService::getAlgorithmName() {
    switch (algorithm) {
        case SchedulerAlgorithm::ROUND_ROBIN: return "Round Robin";
        case SchedulerAlgorithm::PRIORITY:    return "Priority";
        case SchedulerAlgorithm::SJF:         return "SJF";
        default: return "Unknown";
    }
}

// =====================================================
//  GANTT CHART
// =====================================================

void SchedulerService::printGanttChart() {
    OS_LockGuard lock(printMutex);

    if (executionLog.empty()) {
        cout << "\n  No scheduling data available yet.\n\n";
        return;
    }

    cout << "\n  ================================================\n";
    cout << "   GANTT CHART - " << getAlgorithmName() << " (Quantum = " << timeQuantum << ")\n";
    cout << "  ================================================\n\n";

    cout << "  +";
    for (size_t i = 0; i < executionLog.size(); i++) cout << "------+";
    cout << "\n  |";

    for (auto& e : executionLog) {
        stringstream ss;
        ss << "P" << e.pid;
        string label = ss.str();
        int totalPad = 6 - (int)label.length();
        int leftPad = totalPad / 2;
        int rightPad = totalPad - leftPad;
        cout << string(leftPad, ' ') << label << string(rightPad, ' ') << "|";
    }
    cout << "\n  +";
    for (size_t i = 0; i < executionLog.size(); i++) cout << "------+";
    cout << "\n  ";

    for (size_t i = 0; i <= executionLog.size(); i++) {
        int time = (i == 0) ? executionLog[0].startTime : executionLog[i-1].endTime;
        string ts = to_string(time);
        cout << ts;
        if (i < executionLog.size()) {
            int pad = 7 - (int)ts.length();
            if (pad > 0) cout << string(pad, ' ');
        }
    }
    cout << "\n\n";
}

void SchedulerService::printDetailedLog() {
    OS_LockGuard lock(printMutex);

    if (executionLog.empty()) {
        cout << "  No execution log available.\n\n";
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

void SchedulerService::printStats() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Scheduler Statistics ==========\n";
    cout << "  Algorithm:          " << getAlgorithmName() << "\n";
    cout << "  Time Quantum:       " << timeQuantum << "\n";
    cout << "  Current CPU Time:   " << currentTime << "\n";
    cout << "  Context Switches:   " << totalContextSwitches << "\n";
    cout << "  Switches Skipped:   " << totalSkippedSwitches << " (adaptive optimization)\n";
    cout << "  Processes Completed:" << totalCompletions << "\n";

    int inReady = (int)readyList.size() + (hasCurrent ? 1 : 0);
    int inBlocked = (int)blockedList.size();
    cout << "  Ready Queue:        " << inReady << " processes\n";
    cout << "  Blocked (Suspended):" << inBlocked << " processes\n";

    if (hasCurrent) {
        cout << "  Currently Running:  PID " << currentRunning.pid
             << " (remaining: " << currentRunning.remainingTime
             << ", priority: " << currentRunning.priority << ")\n";
    } else {
        cout << "  Currently Running:  IDLE\n";
    }

    if (!blockedList.empty()) {
        cout << "  Blocked Processes:  ";
        for (auto& p : blockedList) {
            cout << "PID " << p.pid << " ";
        }
        cout << "\n";
    }

    cout << "  Execution Slots:    " << executionLog.size() << "\n";
    cout << "  ==========================================\n\n";
}

// =====================================================
//  JSON EXPORT FOR HTTP API
// =====================================================

string SchedulerService::toJSON() {
    stringstream ss;
    ss << "{";
    ss << "\"algorithm\":\"" << getAlgorithmName() << "\",";
    ss << "\"quantum\":" << timeQuantum << ",";
    ss << "\"effectiveQuantum\":" << lastEffectiveQuantum << ",";
    ss << "\"cpuTime\":" << currentTime << ",";
    ss << "\"contextSwitches\":" << totalContextSwitches << ",";
    ss << "\"completions\":" << totalCompletions << ",";
    ss << "\"currentPid\":" << (hasCurrent ? currentRunning.pid : -1) << ",";

    // Processes
    ss << "\"processes\":[";
    vector<PCB> snapshot = getProcessSnapshot();
    for (size_t i = 0; i < snapshot.size(); i++) {
        auto& p = snapshot[i];
        ss << "{\"pid\":" << p.pid
           << ",\"name\":\"" << p.name << "\""
           << ",\"burst\":" << p.burstTime
           << ",\"remaining\":" << p.remainingTime
           << ",\"priority\":" << p.priority
           << ",\"state\":\"" << p.state << "\"}";
        if (i < snapshot.size() - 1) ss << ",";
    }
    ss << "],";

    // Gantt log
    ss << "\"gantt\":[";
    for (size_t i = 0; i < executionLog.size(); i++) {
        auto& e = executionLog[i];
        ss << "{\"pid\":" << e.pid
           << ",\"start\":" << e.startTime
           << ",\"end\":" << e.endTime << "}";
        if (i < executionLog.size() - 1) ss << ",";
    }
    ss << "]";

    ss << "}";
    return ss.str();
}

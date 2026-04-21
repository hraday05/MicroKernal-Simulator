#ifndef SCHEDULER_SERVICE_H
#define SCHEDULER_SERVICE_H

#include <string>
#include "ProcessServer.h"
#include "Service.h"
#include <vector>

enum class SchedulerAlgorithm { ROUND_ROBIN, PRIORITY, SJF };

struct ScheduleEntry {
    int pid;
    int startTime;
    int endTime;
};

class SchedulerService : public Service {
private:
    std::vector<PCB> readyList;
    std::vector<PCB> blockedList;   // suspended/blocked processes
    int timeQuantum;
    ProcessServer* processServer;
    SchedulerAlgorithm algorithm;

    PCB currentRunning;
    bool hasCurrent;

    std::vector<ScheduleEntry> executionLog;
    int currentTime;
    int totalContextSwitches;
    int totalSkippedSwitches;
    int totalCompletions;

    int pickNextIndex();

public:
    SchedulerService(ProcessServer* ps);
    void handleMessage(Message msg) override;
    void addProcess(PCB p);

    std::vector<PCB> getProcessSnapshot();

    // Process signals
    bool suspendProcess(int pid);
    bool resumeProcess(int pid);
    bool killProcess(int pid);

    // Algorithm management
    void setAlgorithm(SchedulerAlgorithm algo);
    std::string getAlgorithmName();

    // Visualization
    void printGanttChart();
    void printDetailedLog();
    void printStats();

    // JSON export for HTTP API
    std::string toJSON();
    int getCurrentTime() { return currentTime; }
    int getContextSwitches() { return totalContextSwitches; }
    int getCompletions() { return totalCompletions; }
    int getCurrentPid() { return hasCurrent ? currentRunning.pid : -1; }
    int getTimeQuantum() { return timeQuantum; }
    std::vector<ScheduleEntry>& getExecutionLog() { return executionLog; }
};

#endif

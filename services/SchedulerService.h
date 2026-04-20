#ifndef SCHEDULER_SERVICE_H
#define SCHEDULER_SERVICE_H

#include "Service.h"
#include "ProcessServer.h"
#include <queue>
#include <vector>

// Tracks one scheduling time slot for the Gantt chart
struct ScheduleEntry {
    int pid;
    int startTime;
    int endTime;
};

class SchedulerService : public Service {
private:
    std::queue<PCB> readyQueue;
    int timeQuantum;
    ProcessServer* processServer;
    PCB currentRunning;
    bool hasCurrent;

    // Execution tracking for visualization
    std::vector<ScheduleEntry> executionLog;
    int currentTime;
    int totalContextSwitches;
    int totalCompletions;

public:
    SchedulerService(ProcessServer* ps);
    void handleMessage(Message msg) override;
    void addProcess(PCB p);

    // Snapshot for the 'ps' command
    std::vector<PCB> getProcessSnapshot();

    // Visualization & Monitoring
    void printGanttChart();
    void printDetailedLog();
    void printStats();
};

#endif

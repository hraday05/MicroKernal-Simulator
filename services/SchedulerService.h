#ifndef SCHEDULER_SERVICE_H
#define SCHEDULER_SERVICE_H

#include "Service.h"
#include "ProcessServer.h"
#include <queue>
#include <vector>

class SchedulerService : public Service {
private:
    std::queue<PCB> readyQueue;
    int timeQuantum;
    ProcessServer* processServer;
    PCB currentRunning;          // track which process is currently on the CPU
    bool hasCurrent;             // true if a process is actively running
public:
    SchedulerService(ProcessServer* ps);
    void handleMessage(Message msg) override;
    void addProcess(PCB p);

    // New: return a snapshot of all processes in the scheduler (running + ready queue)
    std::vector<PCB> getProcessSnapshot();
};

#endif

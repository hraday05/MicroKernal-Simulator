#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include "../services/ProcessServer.h"
#include "../services/MemoryService.h"

class Scheduler {
private:
    std::queue<Process> readyQueue;
    int timeQuantum;
    ProcessServer* processServer;
    MemoryService* memoryService;   // ✅ ADD THIS

public:
    Scheduler();

    void addProcess(Process p);
    void run();

    void setProcessServer(ProcessServer* ps);
    void setMemoryService(MemoryService* ms);   // ✅ ADD THIS
};

#endif
#ifndef SCHEDULER_SERVICE_H
#define SCHEDULER_SERVICE_H

#include "Service.h"
#include "ProcessServer.h"
#include <queue>

class SchedulerService : public Service {
private:
    std::queue<PCB> readyQueue;
    int timeQuantum;
    ProcessServer* processServer;
public:
    SchedulerService(ProcessServer* ps);
    void handleMessage(Message msg) override;
    void addProcess(PCB p);
};

#endif

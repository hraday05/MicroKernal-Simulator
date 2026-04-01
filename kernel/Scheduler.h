#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include "../services/ProcessServer.h"

class Scheduler {
private:
    std::queue<Process> readyQueue;
    int timeQuantum;

public:
    Scheduler();

    void addProcess(Process p);
    void run();
};

#endif
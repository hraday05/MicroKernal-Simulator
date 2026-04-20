#ifndef PROCESS_SERVER_H
#define PROCESS_SERVER_H

#include <vector>
#include <string>
#include "../ipc/IPC.h"

class PCB {
public:
    int pid;
    std::string name;
    int burstTime;        // original burst time (never changes)
    int remainingTime;    // remaining burst for scheduling (decremented each quantum)
    std::string state;    // READY, RUNNING, BLOCKED, DEAD
    std::vector<int> pageTable;
};

#include "Service.h"

class ProcessServer : public Service {
private:
    std::vector<PCB> processes;
    int nextPID;
    

public:
    ProcessServer();

    void handleMessage(Message msg);

    PCB getLastProcess();

    bool processExists(int pid);

    void removeProcess(int pid);
};



#endif

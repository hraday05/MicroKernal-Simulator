#ifndef PROCESSSERVER_H
#define PROCESSSERVER_H

#include <string>
#include <vector>
#include "../ipc/IPC.h"
#include "Service.h"

class PCB {
public:
    int pid = 0;
    std::string name;
    int burstTime = 0;        // original burst time (never changes)
    int remainingTime = 0;    // remaining burst for scheduling
    int priority = 5;         // lower number = higher priority (1=highest, 10=lowest)
    std::string state;        // READY, RUNNING, BLOCKED, DEAD
    std::vector<int> pageTable;
};

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

    // Process signals
    PCB* findProcess(int pid);
    bool setProcessState(int pid, const std::string& state);
};

#endif

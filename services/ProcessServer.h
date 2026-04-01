#ifndef PROCESS_SERVER_H
#define PROCESS_SERVER_H

#include <vector>
#include <string>
#include "../ipc/IPC.h"

struct Process {
    int pid;
    std::string name;
};

class ProcessServer {
private:
    std::vector<Process> processes;
    int nextPID;

public:
    ProcessServer();

    void handleMessage(Message msg);
};

#endif
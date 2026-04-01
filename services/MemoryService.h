#ifndef MEMORY_SERVICE_H
#define MEMORY_SERVICE_H

#include <map>
#include "../ipc/IPC.h"

class MemoryService {
private:
    std::map<int, int> memoryMap; // pid -> memory
    int totalMemory;
    int usedMemory;

public:
    MemoryService();

    void handleMessage(Message msg);
};

#endif
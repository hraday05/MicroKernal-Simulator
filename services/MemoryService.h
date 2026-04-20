#ifndef MEMORY_SERVICE_H
#define MEMORY_SERVICE_H

#include <map>
#include <vector>
#include "../ipc/IPC.h"
#include "Service.h"

class MemoryService : public Service {
private:
    std::vector<bool> physicalMemory; // false = free, true = used
    int pageSize;
    int totalPages;
    int usedPages;

    // Per-PID frame tracking so we can actually free memory
    std::map<int, std::vector<int>> pidFrameMap;

public:
    MemoryService();

    void handleMessage(Message msg) override;
    void freeAll(int pid);
    void printStats();   // New: show memory usage
};

#endif

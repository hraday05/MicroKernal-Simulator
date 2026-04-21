#ifndef MEMORY_SERVICE_H
#define MEMORY_SERVICE_H

#include <vector>
#include <string>
#include "../ipc/IPC.h"
#include "Service.h"

enum class MemAlgorithm { FIRST_FIT, BEST_FIT, WORST_FIT };

struct MemBlock {
    int startFrame;
    int size;       // in pages
    bool free;
    int ownerPid;   // -1 if free
};

class MemoryService : public Service {
private:
    std::vector<MemBlock> blocks;
    int pageSize;
    int totalPages;
    MemAlgorithm algorithm;

    int findFirstFit(int pagesNeeded);
    int findBestFit(int pagesNeeded);
    int findWorstFit(int pagesNeeded);

    void splitBlock(int blockIndex, int pagesNeeded, int pid);
    void coalesce();

public:
    MemoryService();
    void handleMessage(Message msg) override;
    void freeAll(int pid);
    void printStats();
    void printMemoryMap();
    void setAlgorithm(MemAlgorithm algo);
    std::string getAlgorithmName();

    // JSON export for HTTP API
    std::string toJSON();
};

#endif

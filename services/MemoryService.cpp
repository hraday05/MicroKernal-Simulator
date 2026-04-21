#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include "MemoryService.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/Globals.h"

using namespace std;

MemoryService::MemoryService() {
    pageSize = 4096;      // 4KB
    totalPages = 256;     // 1MB total
    algorithm = MemAlgorithm::FIRST_FIT;

    // Start with one big free block
    MemBlock initial;
    initial.startFrame = 0;
    initial.size = totalPages;
    initial.free = true;
    initial.ownerPid = -1;
    blocks.push_back(initial);
}

// =====================================================
//  ALLOCATION ALGORITHMS
// =====================================================

int MemoryService::findFirstFit(int pagesNeeded) {
    for (int i = 0; i < (int)blocks.size(); i++) {
        if (blocks[i].free && blocks[i].size >= pagesNeeded) {
            return i;
        }
    }
    return -1;
}

int MemoryService::findBestFit(int pagesNeeded) {
    int bestIdx = -1;
    int bestSize = 999999;
    for (int i = 0; i < (int)blocks.size(); i++) {
        if (blocks[i].free && blocks[i].size >= pagesNeeded) {
            if (blocks[i].size < bestSize) {
                bestSize = blocks[i].size;
                bestIdx = i;
            }
        }
    }
    return bestIdx;
}

int MemoryService::findWorstFit(int pagesNeeded) {
    int worstIdx = -1;
    int worstSize = -1;
    for (int i = 0; i < (int)blocks.size(); i++) {
        if (blocks[i].free && blocks[i].size >= pagesNeeded) {
            if (blocks[i].size > worstSize) {
                worstSize = blocks[i].size;
                worstIdx = i;
            }
        }
    }
    return worstIdx;
}

// =====================================================
//  BLOCK MANAGEMENT
// =====================================================

void MemoryService::splitBlock(int blockIndex, int pagesNeeded, int pid) {
    MemBlock& block = blocks[blockIndex];

    if (block.size > pagesNeeded) {
        // Create remainder block
        MemBlock remainder;
        remainder.startFrame = block.startFrame + pagesNeeded;
        remainder.size = block.size - pagesNeeded;
        remainder.free = true;
        remainder.ownerPid = -1;

        // Shrink current block to needed size
        block.size = pagesNeeded;
        block.free = false;
        block.ownerPid = pid;

        // Insert remainder after current
        blocks.insert(blocks.begin() + blockIndex + 1, remainder);
    } else {
        // Exact fit
        block.free = false;
        block.ownerPid = pid;
    }
}

void MemoryService::coalesce() {
    for (int i = 0; i < (int)blocks.size() - 1; ) {
        if (blocks[i].free && blocks[i+1].free) {
            blocks[i].size += blocks[i+1].size;
            blocks.erase(blocks.begin() + i + 1);
        } else {
            i++;
        }
    }
}

// =====================================================
//  MESSAGE HANDLER
// =====================================================

void MemoryService::handleMessage(Message msg) {
    if (msg.type == "memory") {
        int amount = 0;
        try { amount = stoi(msg.data); } catch(...) { return; }

        int pagesNeeded = (amount + pageSize - 1) / pageSize;
        int pid = msg.sender;

        // Find suitable block using current algorithm
        int blockIdx = -1;
        switch (algorithm) {
            case MemAlgorithm::FIRST_FIT: blockIdx = findFirstFit(pagesNeeded); break;
            case MemAlgorithm::BEST_FIT:  blockIdx = findBestFit(pagesNeeded);  break;
            case MemAlgorithm::WORST_FIT: blockIdx = findWorstFit(pagesNeeded); break;
        }

        if (blockIdx == -1) {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] ERROR: No suitable block found ("
                 << getAlgorithmName() << ") for PID " << pid << " — OOM!\n";
            return;
        }

        int startFrame = blocks[blockIdx].startFrame;
        splitBlock(blockIdx, pagesNeeded, pid);

        {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] Allocated " << pagesNeeded
                 << " pages at Frame " << startFrame
                 << " for PID " << pid
                 << " (" << getAlgorithmName() << ")\n";
        }

    } else if (msg.type == "free" || msg.type == "process_dead") {
        freeAll(msg.sender);
    }
}

void MemoryService::freeAll(int pid) {
    int freed = 0;
    for (auto& block : blocks) {
        if (!block.free && block.ownerPid == pid) {
            block.free = true;
            block.ownerPid = -1;
            freed += block.size;
        }
    }
    coalesce();

    if (freed > 0) {
        OS_LockGuard lock(printMutex);
        cout << "[MemoryService] Freed " << freed << " pages for PID " << pid << "\n";
    }
}

// =====================================================
//  ALGORITHM MANAGEMENT
// =====================================================

void MemoryService::setAlgorithm(MemAlgorithm algo) {
    algorithm = algo;
    OS_LockGuard lock(printMutex);
    cout << "[MemoryService] Algorithm changed to: " << getAlgorithmName() << "\n";
}

string MemoryService::getAlgorithmName() {
    switch (algorithm) {
        case MemAlgorithm::FIRST_FIT: return "FIRST FIT";
        case MemAlgorithm::BEST_FIT:  return "BEST FIT";
        case MemAlgorithm::WORST_FIT: return "WORST FIT";
        default: return "UNKNOWN";
    }
}

// =====================================================
//  MEMORY MAP VISUALIZATION
// =====================================================

void MemoryService::printMemoryMap() {
    OS_LockGuard lock(printMutex);

    int usedPages = 0, freePages = 0, freeBlocks = 0;
    for (auto& b : blocks) {
        if (b.free) { freePages += b.size; freeBlocks++; }
        else usedPages += b.size;
    }

    cout << "\n  ================================================================\n";
    cout << "   MEMORY MAP (" << totalPages << " pages, "
         << (totalPages * pageSize / 1024) << " KB) - " << getAlgorithmName() << "\n";
    cout << "  ================================================================\n\n";

    // Visual bar (each char = ~4 pages, max 64 chars wide)
    const int MAP_WIDTH = 64;
    int pagesPerChar = (totalPages + MAP_WIDTH - 1) / MAP_WIDTH;

    cout << "  [";
    for (auto& b : blocks) {
        int chars = b.size / pagesPerChar;
        if (chars < 1) chars = 1;
        char fill = b.free ? '.' : '#';
        for (int i = 0; i < chars; i++) cout << fill;
    }
    cout << "]\n";

    // Legend line
    cout << "   ";
    for (auto& b : blocks) {
        int chars = b.size / pagesPerChar;
        if (chars < 1) chars = 1;

        string label;
        if (b.free) {
            label = "FREE";
        } else {
            label = "P" + to_string(b.ownerPid);
        }

        if ((int)label.length() <= chars) {
            cout << label;
            for (int i = (int)label.length(); i < chars; i++) cout << " ";
        } else {
            cout << label.substr(0, chars);
        }
    }
    cout << "\n\n";

    // Block table
    cout << "  +--------+--------+------+--------+\n";
    cout << "  | Frame  |  Size  | PID  | Status |\n";
    cout << "  +--------+--------+------+--------+\n";

    for (auto& b : blocks) {
        cout << "  | " << setw(6) << b.startFrame << " | "
             << setw(3) << b.size << " pg | ";
        if (b.free) {
            cout << setw(4) << "-" << " |  FREE  |\n";
        } else {
            cout << setw(4) << b.ownerPid << " |  USED  |\n";
        }
    }

    cout << "  +--------+--------+------+--------+\n\n";

    // Stats
    cout << "  Used: " << usedPages << " pages (" << (usedPages * pageSize / 1024) << " KB)"
         << " | Free: " << freePages << " pages (" << (freePages * pageSize / 1024) << " KB)\n";

    if (freeBlocks > 1) {
        cout << "  External Fragmentation: " << freeBlocks
             << " non-contiguous free blocks\n";
    } else {
        cout << "  External Fragmentation: None\n";
    }

    cout << "  ================================================================\n\n";
}

void MemoryService::printStats() {
    OS_LockGuard lock(printMutex);

    int usedPages = 0, freePages = 0;
    for (auto& b : blocks) {
        if (b.free) freePages += b.size;
        else usedPages += b.size;
    }

    cout << "\n========== Memory Statistics ==========\n";
    cout << "  Algorithm:     " << getAlgorithmName() << "\n";
    cout << "  Page Size:     " << pageSize << " bytes (4 KB)\n";
    cout << "  Total Pages:   " << totalPages << "\n";
    cout << "  Used Pages:    " << usedPages << "\n";
    cout << "  Free Pages:    " << freePages << "\n";
    cout << "  Total RAM:     " << (totalPages * pageSize / 1024) << " KB\n";
    cout << "  Used RAM:      " << (usedPages * pageSize / 1024) << " KB\n";
    cout << "  Free RAM:      " << (freePages * pageSize / 1024) << " KB\n";
    cout << "  Memory Blocks: " << blocks.size() << "\n";
    cout << "---------------------------------------\n";

    // Per-PID summary
    map<int, int> pidTotals;
    for (auto& b : blocks) {
        if (!b.free) pidTotals[b.ownerPid] += b.size;
    }

    if (!pidTotals.empty()) {
        cout << "  Per-Process Allocation:\n";
        for (auto& entry : pidTotals) {
            cout << "    PID " << setw(5) << entry.first
                 << " : " << entry.second << " pages ("
                 << (entry.second * pageSize / 1024) << " KB)\n";
        }
    } else {
        cout << "  No memory allocated to any process.\n";
    }
    cout << "=======================================\n\n";
}

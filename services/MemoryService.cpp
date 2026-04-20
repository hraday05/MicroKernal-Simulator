#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "../kernel/OS_Mutex.h"
#include "MemoryService.h"
#include "../kernel/Globals.h"

using namespace std;

MemoryService::MemoryService() {
    pageSize = 4096;      // 4KB page size
    totalPages = 256;     // 1MB total RAM simulation
    usedPages = 0;
    physicalMemory.assign(totalPages, false); 
}

void MemoryService::handleMessage(Message msg) {
    if (msg.type == "memory") {
        int amount = 0;
        try {
            amount = stoi(msg.data);
        } catch(...) { return; }

        int pagesNeeded = (amount + pageSize - 1) / pageSize; 
        
        if (usedPages + pagesNeeded > totalPages) {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] ERROR: OOM for PID: " << msg.sender << "\n";
            return;
        }

        vector<int> allocatedFrames;
        for (int i = 0; i < totalPages; i++) {
            if (!physicalMemory[i]) {
                allocatedFrames.push_back(i);
                physicalMemory[i] = true;
                usedPages++;
                if ((int)allocatedFrames.size() == pagesNeeded) break;
            }
        }

        // Track which frames belong to which PID
        for (int f : allocatedFrames) {
            pidFrameMap[msg.sender].push_back(f);
        }
        
        {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] Allocated " << allocatedFrames.size() 
                 << " pages (Frames";
            for(int f : allocatedFrames) cout << " " << f;
            cout << ") for PID: " << msg.sender << "\n";
        }

    } else if (msg.type == "free") {
        int amount = 0;
        try {
            amount = stoi(msg.data);
        } catch(...) { return; }

        int pagesToFree = (amount + pageSize - 1) / pageSize;
        int pid = msg.sender;

        if (pidFrameMap.find(pid) == pidFrameMap.end() || pidFrameMap[pid].empty()) {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] ERROR: PID " << pid << " has no allocated memory\n";
            return;
        }

        vector<int>& frames = pidFrameMap[pid];
        int freed = 0;

        while (freed < pagesToFree && !frames.empty()) {
            int frame = frames.back();
            frames.pop_back();
            physicalMemory[frame] = false;
            usedPages--;
            freed++;
        }

        {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] Freed " << freed << " pages for PID: " << pid << "\n";
        }

    } else if (msg.type == "process_dead") {
        freeAll(msg.sender);
    }
}

void MemoryService::freeAll(int pid) {
    if (pidFrameMap.find(pid) == pidFrameMap.end()) return;

    vector<int>& frames = pidFrameMap[pid];
    int count = (int)frames.size();

    for (int f : frames) {
        physicalMemory[f] = false;
        usedPages--;
    }
    frames.clear();
    pidFrameMap.erase(pid);

    OS_LockGuard lock(printMutex);
    cout << "[MemoryService] Freed ALL (" << count << " pages) for PID: " << pid << "\n";
}

void MemoryService::printStats() {
    OS_LockGuard lock(printMutex);
    cout << "\n========== Memory Statistics ==========\n";
    cout << "  Page Size:     " << pageSize << " bytes (4 KB)\n";
    cout << "  Total Pages:   " << totalPages << "\n";
    cout << "  Used Pages:    " << usedPages << "\n";
    cout << "  Free Pages:    " << (totalPages - usedPages) << "\n";
    cout << "  Total RAM:     " << (totalPages * pageSize / 1024) << " KB\n";
    cout << "  Used RAM:      " << (usedPages * pageSize / 1024) << " KB\n";
    cout << "  Free RAM:      " << ((totalPages - usedPages) * pageSize / 1024) << " KB\n";
    cout << "---------------------------------------\n";

    if (!pidFrameMap.empty()) {
        cout << "  Per-Process Allocation:\n";
        for (auto& entry : pidFrameMap) {
            cout << "    PID " << setw(5) << entry.first
                 << " : " << entry.second.size() << " pages ("
                 << (entry.second.size() * pageSize / 1024) << " KB)\n";
        }
    } else {
        cout << "  No memory allocated to any process.\n";
    }
    cout << "=======================================\n\n";
}

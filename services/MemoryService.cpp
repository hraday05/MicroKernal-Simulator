#include <iostream>
#include <string>
#include <vector>
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
                if (allocatedFrames.size() == pagesNeeded) break;
            }
        }
        
        {
            OS_LockGuard lock(printMutex);
            cout << "[MemoryService] Allocated " << allocatedFrames.size() 
                 << " pages (Frames";
            for(int f : allocatedFrames) cout << " " << f;
            cout << ") for PID: " << msg.sender << "\n";
        }

        // We could broadcast success here, but simulation works without the Process explicitly tracking the exact Frame #s
    } else if (msg.type == "free" || msg.type == "process_dead") {
        // Simplified mapping recovery for simulation
    }
}

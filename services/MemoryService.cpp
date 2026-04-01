#include <iostream>
#include "MemoryService.h"
#include "../kernel/Globals.h"

using namespace std;

MemoryService::MemoryService() {
    totalMemory = 1000; // total memory
    usedMemory = 0;
}

void MemoryService::handleMessage(Message msg) {

   if (msg.type == "memory") {

    int pid = msg.sender;

    if (msg.data.empty()) {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] Invalid allocation request\n";
        return;
    }

    try {
        int amount = stoi(msg.data);

        if (amount <= 0) {
            std::lock_guard<std::mutex> lock(printMutex);
            cout << "[MemoryService] Invalid memory amount\n";
            return;
        }

        if (usedMemory + amount > totalMemory) {
            std::lock_guard<std::mutex> lock(printMutex);
            cout << "[MemoryService] Not enough memory\n";
            return;
        }

        memoryMap[pid] += amount;
        usedMemory += amount;

        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] Allocated "
             << amount << " to PID " << pid << endl;
        cout << "[MemoryService] Used: "
             << usedMemory << "/" << totalMemory << endl;

    } catch (...) {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] Invalid input (not a number)\n";
    }
   }

    else if (msg.type == "free") {

    int pid = msg.sender;

    if (memoryMap.find(pid) == memoryMap.end()) {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] No memory allocated for PID "
             << pid << endl;
        return;
    }

    int amount;

    try {
        amount = stoi(msg.data);
    } catch (...) {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] Invalid free amount\n";
        return;
    }

    amount = min(amount, memoryMap[pid]);

    memoryMap[pid] -= amount;
    usedMemory -= amount;

    if (memoryMap[pid] == 0) {
        memoryMap.erase(pid);
    }

    std::lock_guard<std::mutex> lock(printMutex);
    cout << "[MemoryService] Freed "
         << amount << " from PID " << pid << endl;
 }
}

void MemoryService::freeAll(int pid) {
    if (memoryMap.find(pid) != memoryMap.end()) {
        usedMemory -= memoryMap[pid];
        memoryMap.erase(pid);

        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[MemoryService] Auto-freed memory of PID "
             << pid << endl;
    }
}
#include <iostream>
#include "Kernel.h"

using namespace std;

Kernel::Kernel() {
    running = false;
}

void Kernel::sendMessage(Message msg) {
    messageQueue.push(msg);
}

void Kernel::processMessages() {
    while (!messageQueue.empty()) {
        Message msg = messageQueue.front();
        messageQueue.pop();

        {
        std::lock_guard<std::mutex> lock(printMutex);
        cout << "[Kernel] Routing message...\n";
        }

        if (msg.type == "command" && msg.data == "create_process") {
            processServer.handleMessage(msg);

            // Get last created process
            Process p = processServer.getLastProcess(); // we add this next
            scheduler.addProcess(p);
        }

        else if (msg.type == "memory" || msg.type == "free") {
            memoryService.handleMessage(msg);
        }

        else {
            processServer.handleMessage(msg);
        }
    }
}

void Kernel::startScheduler() {
    if (running) return;   // 🚨 prevent duplicate threads
    running = true;

     schedulerThread = std::thread([this]() {
        while (running) {
            scheduler.run();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });
}

void Kernel::stopScheduler() {
    running = false;

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
}


#include <iostream>
#include "Kernel.h"

using namespace std;

Kernel::Kernel() : schedulerService(&processServer) {
    running = false;
    processServer.setBus(&messageBus);
    memoryService.setBus(&messageBus);
    fileService.setBus(&messageBus);
    schedulerService.setBus(&messageBus);
    securityServer.setBus(&messageBus);
}

void Kernel::sendMessage(Message msg) {
    messageBus.sendMessage(msg);
}

void Kernel::processMessages() {
    while (messageBus.hasMessages()) {
        Message msg = messageBus.receiveMessage();

        {
        OS_LockGuard lock(printMutex);
        cout << "[Kernel] Routing message...\n";
        }

        if (msg.type == "command" && msg.data == "create_process") {
            processServer.handleMessage(msg);

            // Get last created process
            PCB p = processServer.getLastProcess();
            schedulerService.addProcess(p);
        }

                 else if (msg.type == "memory" || msg.type == "free") {
                     if (processServer.processExists(msg.sender)) {
                      memoryService.handleMessage(msg);
                    } else {
                      OS_LockGuard lock(printMutex);
                      cout << "[Kernel] ERROR: PID "
                           << msg.sender << " does not exist\n";
                  }
        }

        else if (msg.type == "command") {
            processServer.handleMessage(msg);
        }
        else if (msg.type == "file") {
            fileService.handleMessage(msg);
        }
        else if (msg.type == "interrupt") {
            schedulerService.handleMessage(msg);
        }
        else if (msg.type == "request_capability") {
            securityServer.handleMessage(msg);
        }
        else if (msg.type == "kill_service") {
            OS_LockGuard lock(printMutex);
            cout << "[WATCHDOG] CRITICAL FAULT DETECTED: " << msg.data << " crashed!\n";
            cout << "[WATCHDOG] Restarting " << msg.data << " isolating fault entirely...\n";
            if (msg.data == "FileService") fileService = FileService();
        }
        else {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Unknown message type\n";
        }
    }
}

DWORD WINAPI KernelSchedulerBody(LPVOID param) {
    Kernel* k = (Kernel*)param;
    while (k->isRunning()) {
        Message interruptMsg;
        interruptMsg.sender = 0; // Hardware/Kernel
        interruptMsg.type = "interrupt";
        interruptMsg.data = "timer";
        k->sendMessage(interruptMsg);
        Sleep(1000);
    }
    return 0;
}

void Kernel::startScheduler() {
    if (running) return;   // 🚨 prevent duplicate threads
    running = true;

    schedulerThread.start(KernelSchedulerBody, this);
}

void Kernel::stopScheduler() {
    running = false;

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
}



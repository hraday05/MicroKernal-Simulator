#include <iostream>
#include "Kernel.h"
#include "Globals.h"
#include "OS_Mutex.h"

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

        // Only print routing log for user-initiated messages, not background timer ticks
        if (msg.type != "interrupt") {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Routing message type='" << msg.type << "'...\n";
        }

        if (msg.type == "command" && msg.data.find("create_process") == 0) {
            processServer.handleMessage(msg);

            // Get last created process and add to scheduler
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
        else if (msg.type == "process_dead") {
            memoryService.handleMessage(msg);
        }
        else if (msg.type == "request_capability") {
            securityServer.handleMessage(msg);
        }
        else if (msg.type == "kill_service") {
            OS_LockGuard lock(printMutex);
            cout << "[WATCHDOG] CRITICAL FAULT DETECTED: " << msg.data << " crashed!\n";
            cout << "[WATCHDOG] Restarting " << msg.data << " isolating fault entirely...\n";
            if (msg.data == "FileService") {
                fileService = FileService();
                fileService.setBus(&messageBus);  // FIX: rebind the message bus!
            }
        }
        else {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Unknown message type\n";
        }
    }
}

// Background thread: sends timer interrupts AND processes all pending messages
DWORD WINAPI KernelSchedulerBody(LPVOID param) {
    Kernel* k = (Kernel*)param;
    while (k->isRunning()) {
        // Generate a timer interrupt
        Message interruptMsg;
        interruptMsg.sender = 0; // Hardware/Kernel
        interruptMsg.type = "interrupt";
        interruptMsg.data = "timer";
        k->sendMessage(interruptMsg);

        // Process ALL pending messages (scheduler ticks, etc.)
        k->processMessages();

        Sleep(1000);
    }
    return 0;
}

void Kernel::startScheduler() {
    if (running) return;   // prevent duplicate threads
    running = true;

    schedulerThread.start(KernelSchedulerBody, this);
}

void Kernel::stopScheduler() {
    running = false;

    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
}

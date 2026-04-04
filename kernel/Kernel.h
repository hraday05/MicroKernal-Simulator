#ifndef KERNEL_H
#define KERNEL_H

#include <queue>
#include "../ipc/IPC.h"
#include "../services/ProcessServer.h"
#include "../services/MemoryService.h"
#include "../services/FileService.h"
#include "../kernel/OS_Thread.h"
#include <atomic>
#include "../kernel/OS_Mutex.h"
#include "../services/SchedulerService.h"
#include "../services/SecurityServer.h"

class Kernel {
private:
    MessageBus messageBus;
    ProcessServer processServer;
    MemoryService memoryService;
    FileService fileService;
    SchedulerService schedulerService;
    SecurityServer securityServer;

    OS_Thread schedulerThread;
    std::atomic<bool> running;
    OS_Mutex printMutex;

public:
    Kernel();

    void sendMessage(Message msg);
    void processMessages();

    void startScheduler();
    void stopScheduler();

    bool isRunning() { return running; }
};

#endif

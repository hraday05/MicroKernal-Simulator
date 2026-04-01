#ifndef KERNEL_H
#define KERNEL_H

#include <queue>
#include "../ipc/IPC.h"
#include "../services/ProcessServer.h"
#include <thread>
#include <atomic>
#include <mutex>
#include "Scheduler.h"

class Kernel {
private:
    std::queue<Message> messageQueue;
    ProcessServer processServer;
    Scheduler scheduler;

    std::thread schedulerThread;
    std::atomic<bool> running;
    std::mutex printMutex;

public:
    Kernel();

    void sendMessage(Message msg);
    void processMessages();

    void startScheduler();
    void stopScheduler();
};

#endif
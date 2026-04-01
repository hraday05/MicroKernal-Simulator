#ifndef KERNEL_H
#define KERNEL_H

#include <queue>
#include "../ipc/IPC.h"
#include "../services/ProcessServer.h"

class Kernel {
private:
    std::queue<Message> messageQueue;
    ProcessServer processServer;

public:
    void sendMessage(Message msg);
    void processMessages();
};

#endif
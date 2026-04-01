#ifndef KERNEL_H
#define KERNEL_H

#include <queue>
#include "../ipc/IPC.h"

class Kernel {
private:
    std::queue<Message> messageQueue;

public:
    void sendMessage(Message msg);
    void processMessages();
};

#endif
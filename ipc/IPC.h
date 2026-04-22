#ifndef IPC_H
#define IPC_H

#include <string>
#include <queue>
#include "../kernel/OS_Mutex.h"

struct Message {
    int sender = 0;
    int receiver = 0;
    std::string type;
    std::string data;
    std::string capabilityToken;
};

class MessageBus {
private:
    std::queue<Message> messageQueue;
    OS_Mutex queueMutex;
public:
    void sendMessage(Message msg);
    bool hasMessages();
    Message receiveMessage();
};

#endif

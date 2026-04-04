#ifndef SERVICE_H
#define SERVICE_H

#include "../ipc/IPC.h"

class Service {
protected:
    MessageBus* bus;
public:
    virtual ~Service() {}
    virtual void handleMessage(Message msg) = 0;
    void setBus(MessageBus* b) { bus = b; }
};

#endif

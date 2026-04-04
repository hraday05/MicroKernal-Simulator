#ifndef SECURITY_SERVER_H
#define SECURITY_SERVER_H

#include <unordered_map>
#include <string>
#include "../ipc/IPC.h"
#include "Service.h"

class SecurityServer : public Service {
private:
    std::unordered_map<int, std::string> processCapabilities;
public:
    void handleMessage(Message msg) override;
};

#endif

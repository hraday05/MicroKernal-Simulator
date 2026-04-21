#ifndef SECURITY_SERVER_H
#define SECURITY_SERVER_H

#include <map>
#include <set>
#include <string>
#include "../ipc/IPC.h"
#include "Service.h"

class SecurityServer : public Service {
private:
    std::map<int, std::set<std::string>> processCapabilities;

public:
    void handleMessage(Message msg) override;

    void grantCapability(int pid, const std::string& cap);
    void revokeCapability(int pid, const std::string& cap);
    bool hasCapability(int pid, const std::string& cap);
    void initDefaultCapabilities(int pid);
    void removeProcess(int pid);
    void printCapabilities(int pid);
    void printAllCapabilities();
};

#endif

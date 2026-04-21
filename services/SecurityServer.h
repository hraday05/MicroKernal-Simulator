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
    void initShellCapabilities(int pid);   // Shell gets ALL capabilities
    void removeProcess(int pid);
    void printCapabilities(int pid);
    void printAllCapabilities();

    // Master sandbox validation — checks capability for ANY message type
    bool validateMessage(int senderPid, const Message& msg);

    // JSON export for HTTP API
    std::string toJSON();
};

#endif

#include "SecurityServer.h"
#include "../kernel/Globals.h"
#include <iostream>
#include <iomanip>

using namespace std;

void SecurityServer::handleMessage(Message msg) {
    if (msg.type == "request_capability") {
        grantCapability(msg.sender, msg.data);
    }
}

void SecurityServer::initDefaultCapabilities(int pid) {
    processCapabilities[pid].insert("CAP_FILE");
    processCapabilities[pid].insert("CAP_MEM");
}

void SecurityServer::grantCapability(int pid, const string& cap) {
    processCapabilities[pid].insert(cap);
    OS_LockGuard lock(printMutex);
    cout << "[SecurityServer] GRANTED '" << cap << "' to PID " << pid << "\n";
}

void SecurityServer::revokeCapability(int pid, const string& cap) {
    if (processCapabilities.find(pid) != processCapabilities.end()) {
        processCapabilities[pid].erase(cap);
    }
    OS_LockGuard lock(printMutex);
    cout << "[SecurityServer] REVOKED '" << cap << "' from PID " << pid << "\n";
}

bool SecurityServer::hasCapability(int pid, const string& cap) {
    if (processCapabilities.find(pid) == processCapabilities.end()) {
        return false;
    }
    return processCapabilities[pid].count(cap) > 0;
}

void SecurityServer::removeProcess(int pid) {
    processCapabilities.erase(pid);
}

void SecurityServer::printCapabilities(int pid) {
    OS_LockGuard lock(printMutex);

    if (processCapabilities.find(pid) == processCapabilities.end()) {
        cout << "\n  PID " << pid << " has NO capabilities (unknown process)\n\n";
        return;
    }

    auto& caps = processCapabilities[pid];
    cout << "\n  Capabilities for PID " << pid << ":\n";
    cout << "  +-------------------+--------+\n";
    cout << "  |   Capability      | Status |\n";
    cout << "  +-------------------+--------+\n";
    cout << "  | CAP_FILE          | " << (caps.count("CAP_FILE") ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_MEM           | " << (caps.count("CAP_MEM")  ? "  YES " : "  NO  ") << " |\n";
    cout << "  +-------------------+--------+\n\n";
}

void SecurityServer::printAllCapabilities() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Capability Table ==========\n";
    cout << "  +-------+-----------+---------+\n";
    cout << "  |  PID  | CAP_FILE  | CAP_MEM |\n";
    cout << "  +-------+-----------+---------+\n";

    if (processCapabilities.empty()) {
        cout << "  |     No processes registered       |\n";
    } else {
        for (auto& entry : processCapabilities) {
            cout << "  | " << setw(5) << entry.first << " | "
                 << setw(9) << (entry.second.count("CAP_FILE") ? "YES" : "NO") << " | "
                 << setw(7) << (entry.second.count("CAP_MEM") ? "YES" : "NO") << " |\n";
        }
    }

    cout << "  +-------+-----------+---------+\n";
    cout << "  =====================================\n\n";
}

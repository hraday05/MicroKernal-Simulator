#include "SecurityServer.h"
#include "../kernel/Globals.h"
#include "../kernel/Logger.h"
#include <iostream>
#include <iomanip>

using namespace std;

void SecurityServer::handleMessage(Message msg) {
    if (msg.type == "request_capability") {
        grantCapability(msg.sender, msg.data);
    }
    else if (msg.type == "security_grant") {
        // data format: "pid cap" e.g. "100 CAP_FILE"
        // Parsed by kernel before forwarding
        grantCapability(msg.receiver, msg.data);
    }
    else if (msg.type == "security_revoke") {
        revokeCapability(msg.receiver, msg.data);
    }
}

// =====================================================
//  CAPABILITY INITIALIZATION
// =====================================================

void SecurityServer::initDefaultCapabilities(int pid) {
    // Normal processes get basic capabilities only
    processCapabilities[pid].insert("CAP_FILE");
    processCapabilities[pid].insert("CAP_MEM");
    processCapabilities[pid].insert("CAP_IPC");
    // Normal processes do NOT get CAP_PROC, CAP_SCHED, CAP_KILL
}

void SecurityServer::initShellCapabilities(int pid) {
    // Shell (PID 1) gets ALL capabilities — it's the trusted admin
    processCapabilities[pid].insert("CAP_FILE");
    processCapabilities[pid].insert("CAP_MEM");
    processCapabilities[pid].insert("CAP_IPC");
    processCapabilities[pid].insert("CAP_PROC");
    processCapabilities[pid].insert("CAP_SCHED");
    processCapabilities[pid].insert("CAP_KILL");
}

// =====================================================
//  CAPABILITY MANAGEMENT
// =====================================================

void SecurityServer::grantCapability(int pid, const string& cap) {
    processCapabilities[pid].insert(cap);
    OS_LockGuard lock(printMutex);
    cout << "[SecurityServer] GRANTED '" << cap << "' to PID " << pid << "\n";
    sysLogger.log("[SecurityServer]", "GRANTED '" + cap + "' to PID " + to_string(pid));
}

void SecurityServer::revokeCapability(int pid, const string& cap) {
    if (processCapabilities.find(pid) != processCapabilities.end()) {
        processCapabilities[pid].erase(cap);
    }
    OS_LockGuard lock(printMutex);
    cout << "[SecurityServer] REVOKED '" << cap << "' from PID " << pid << "\n";
    sysLogger.log("[SecurityServer]", "REVOKED '" + cap + "' from PID " + to_string(pid));
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

// =====================================================
//  MASTER SANDBOX VALIDATION
// =====================================================

bool SecurityServer::validateMessage(int senderPid, const Message& msg) {
    // Kernel (PID 0) is always allowed
    if (senderPid == 0) return true;

    // Determine required capability based on message type
    string requiredCap = "";

    if (msg.type == "file") {
        requiredCap = "CAP_FILE";
    }
    else if (msg.type == "memory" || msg.type == "free") {
        requiredCap = "CAP_MEM";
    }
    else if (msg.type == "command") {
        // Process creation/management requires CAP_PROC
        if (msg.data.find("create_process") == 0 || msg.data == "list_process") {
            requiredCap = "CAP_PROC";
        }
    }
    else if (msg.type == "signal") {
        // kill/suspend/resume requires CAP_KILL
        requiredCap = "CAP_KILL";
    }
    else if (msg.type == "security_grant" || msg.type == "security_revoke") {
        // Only admin (shell) should grant/revoke
        requiredCap = "CAP_KILL";  // Reuse highest privilege
    }
    else if (msg.type == "kill_service") {
        requiredCap = "CAP_KILL";
    }

    // If no specific capability is required, allow
    if (requiredCap.empty()) return true;

    // Check if process has the required capability
    if (!hasCapability(senderPid, requiredCap)) {
        OS_LockGuard lock(printMutex);
        cout << "[Sandbox] DENIED: PID " << senderPid
             << " — lacks " << requiredCap
             << " for '" << msg.type << "' operation\n";
        sysLogger.log("[Sandbox]", "DENIED PID " + to_string(senderPid) +
                      " — lacks " + requiredCap + " for '" + msg.type + "'");
        return false;
    }

    return true;
}

// =====================================================
//  DISPLAY
// =====================================================

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
    cout << "  | CAP_FILE          | " << (caps.count("CAP_FILE")  ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_MEM           | " << (caps.count("CAP_MEM")   ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_IPC           | " << (caps.count("CAP_IPC")   ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_PROC          | " << (caps.count("CAP_PROC")  ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_SCHED         | " << (caps.count("CAP_SCHED") ? "  YES " : "  NO  ") << " |\n";
    cout << "  | CAP_KILL          | " << (caps.count("CAP_KILL")  ? "  YES " : "  NO  ") << " |\n";
    cout << "  +-------------------+--------+\n\n";
}

void SecurityServer::printAllCapabilities() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ==================== Capability Table ====================\n";
    cout << "  +-------+-----------+---------+---------+----------+-----------+----------+\n";
    cout << "  |  PID  | CAP_FILE  | CAP_MEM | CAP_IPC | CAP_PROC | CAP_SCHED | CAP_KILL |\n";
    cout << "  +-------+-----------+---------+---------+----------+-----------+----------+\n";

    if (processCapabilities.empty()) {
        cout << "  |                  No processes registered                                  |\n";
    } else {
        for (auto& entry : processCapabilities) {
            cout << "  | " << setw(5) << entry.first << " | "
                 << setw(9) << (entry.second.count("CAP_FILE")  ? "YES" : "NO") << " | "
                 << setw(7) << (entry.second.count("CAP_MEM")   ? "YES" : "NO") << " | "
                 << setw(7) << (entry.second.count("CAP_IPC")   ? "YES" : "NO") << " | "
                 << setw(8) << (entry.second.count("CAP_PROC")  ? "YES" : "NO") << " | "
                 << setw(9) << (entry.second.count("CAP_SCHED") ? "YES" : "NO") << " | "
                 << setw(8) << (entry.second.count("CAP_KILL")  ? "YES" : "NO") << " |\n";
        }
    }

    cout << "  +-------+-----------+---------+---------+----------+-----------+----------+\n";
    cout << "  =================================================================\n\n";
}

// =====================================================
//  JSON EXPORT FOR HTTP API
// =====================================================

string SecurityServer::toJSON() {
    stringstream ss;
    ss << "{";
    int idx = 0;
    for (auto& entry : processCapabilities) {
        if (idx > 0) ss << ",";
        ss << "\"" << entry.first << "\":[";
        int capIdx = 0;
        for (auto& cap : entry.second) {
            if (capIdx > 0) ss << ",";
            ss << "\"" << cap << "\"";
            capIdx++;
        }
        ss << "]";
        idx++;
    }
    ss << "}";
    return ss.str();
}

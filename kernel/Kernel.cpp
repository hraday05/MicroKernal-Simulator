#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include "Kernel.h"
#include "Globals.h"
#include "OS_Mutex.h"
#include "Logger.h"

using namespace std;

Kernel::Kernel() : schedulerService(&processServer) {
    running = false;
    processServer.setBus(&messageBus);
    memoryService.setBus(&messageBus);
    fileService.setBus(&messageBus);
    schedulerService.setBus(&messageBus);
    securityServer.setBus(&messageBus);

    securityServer.initDefaultCapabilities(1); // Shell PID
    sysLogger.log("[Kernel]", "MicroKernel OS v5.0 booted");
}

void Kernel::sendMessage(Message msg) {
    messageBus.sendMessage(msg);
}

void Kernel::processMessages() {
    while (messageBus.hasMessages()) {
        Message msg = messageBus.receiveMessage();

        if (msg.type != "interrupt") {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Routing message type='" << msg.type << "'...\n";
        }

        // ===== CAPABILITY-BASED SANDBOXING =====
        if (msg.sender != 0 && msg.type == "file") {
            if (!securityServer.hasCapability(msg.sender, "CAP_FILE")) {
                OS_LockGuard lock(printMutex);
                cout << "[Sandbox] DENIED: PID " << msg.sender
                     << " — unauthorized FILE operation! (no CAP_FILE)\n";
                sysLogger.log("[Sandbox]", "DENIED PID " + to_string(msg.sender) + " FILE access");
                continue;
            }
        }

        if (msg.sender != 0 && msg.type == "memory") {
            if (!securityServer.hasCapability(msg.sender, "CAP_MEM")) {
                OS_LockGuard lock(printMutex);
                cout << "[Sandbox] DENIED: PID " << msg.sender
                     << " — unauthorized MEMORY operation! (no CAP_MEM)\n";
                sysLogger.log("[Sandbox]", "DENIED PID " + to_string(msg.sender) + " MEM access");
                continue;
            }
        }

        // ===== MESSAGE ROUTING =====
        if (msg.type == "command" && msg.data.find("create_process") == 0) {
            processServer.handleMessage(msg);
            PCB p = processServer.getLastProcess();
            schedulerService.addProcess(p);
            securityServer.initDefaultCapabilities(p.pid);
        }
        else if (msg.type == "memory" || msg.type == "free") {
            if (processServer.processExists(msg.sender)) {
                memoryService.handleMessage(msg);
            } else {
                OS_LockGuard lock(printMutex);
                cout << "[Kernel] ERROR: PID " << msg.sender << " does not exist\n";
            }
        }
        else if (msg.type == "command") {
            processServer.handleMessage(msg);
        }
        else if (msg.type == "file") {
            fileService.handleMessage(msg);
        }
        else if (msg.type == "interrupt") {
            schedulerService.handleMessage(msg);
        }
        else if (msg.type == "process_dead") {
            memoryService.handleMessage(msg);
            securityServer.removeProcess(msg.sender);
        }
        else if (msg.type == "request_capability") {
            securityServer.handleMessage(msg);
        }
        else if (msg.type == "kill_service") {
            OS_LockGuard lock(printMutex);
            cout << "[WATCHDOG] CRITICAL FAULT: " << msg.data << " crashed!\n";
            cout << "[WATCHDOG] Restarting " << msg.data << "...\n";
            sysLogger.log("[WATCHDOG]", msg.data + " crashed and restarted");
            if (msg.data == "FileService") {
                fileService = FileService();
                fileService.setBus(&messageBus);
            }
        }
        else {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Unknown message type: " << msg.type << "\n";
        }
    }
}

// =====================================================
//  PROCESS SIGNALS
// =====================================================

void Kernel::signalProcess(int pid, const string& signal) {
    if (signal == "kill") {
        schedulerService.killProcess(pid);
        sysLogger.log("[Kernel]", "Signal KILL sent to PID " + to_string(pid));
    } else if (signal == "suspend") {
        schedulerService.suspendProcess(pid);
        processServer.setProcessState(pid, "BLOCKED");
        sysLogger.log("[Kernel]", "Signal SUSPEND sent to PID " + to_string(pid));
    } else if (signal == "resume") {
        schedulerService.resumeProcess(pid);
        processServer.setProcessState(pid, "READY");
        sysLogger.log("[Kernel]", "Signal RESUME sent to PID " + to_string(pid));
    }
}

// =====================================================
//  RESOURCE LOCKING + DEADLOCK DETECTION
// =====================================================

bool Kernel::lockResource(int pid, const string& resource) {
    if (resources.find(resource) == resources.end()) {
        // Resource doesn't exist yet — create and lock it
        ResourceLock rl;
        rl.name = resource;
        rl.heldBy = pid;
        resources[resource] = rl;

        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " LOCKED resource '" << resource << "'\n";
        sysLogger.log("[ResourceMgr]", "PID " + to_string(pid) + " locked '" + resource + "'");
        return true;
    }

    ResourceLock& rl = resources[resource];

    if (rl.heldBy == -1) {
        // Resource is free
        rl.heldBy = pid;
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " LOCKED resource '" << resource << "'\n";
        sysLogger.log("[ResourceMgr]", "PID " + to_string(pid) + " locked '" + resource + "'");
        return true;
    }

    if (rl.heldBy == pid) {
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " already holds '" << resource << "'\n";
        return true;
    }

    // Resource is held by another PID — add to waiters
    rl.waiters.push_back(pid);
    {
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " WAITING for '" << resource
             << "' (held by PID " << rl.heldBy << ")\n";
    }
    sysLogger.log("[ResourceMgr]", "PID " + to_string(pid) + " waiting for '" +
                  resource + "' held by PID " + to_string(rl.heldBy));

    // Check for deadlock after adding waiter
    if (detectDeadlock()) {
        OS_LockGuard lock(printMutex);
        cout << "\n  !! DEADLOCK DETECTED !!\n\n";
        sysLogger.log("[ResourceMgr]", "*** DEADLOCK DETECTED ***");
    }

    return false;
}

bool Kernel::unlockResource(int pid, const string& resource) {
    if (resources.find(resource) == resources.end()) {
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] Resource '" << resource << "' does not exist.\n";
        return false;
    }

    ResourceLock& rl = resources[resource];

    if (rl.heldBy != pid) {
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " does not hold '" << resource << "'.\n";
        return false;
    }

    // Release the lock
    rl.heldBy = -1;
    {
        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] PID " << pid << " RELEASED resource '" << resource << "'\n";
    }
    sysLogger.log("[ResourceMgr]", "PID " + to_string(pid) + " released '" + resource + "'");

    // Grant to first waiter if any
    if (!rl.waiters.empty()) {
        int nextPid = rl.waiters.front();
        rl.waiters.erase(rl.waiters.begin());
        rl.heldBy = nextPid;

        OS_LockGuard lock(printMutex);
        cout << "[ResourceMgr] Resource '" << resource
             << "' granted to waiting PID " << nextPid << "\n";
        sysLogger.log("[ResourceMgr]", "'" + resource + "' granted to PID " + to_string(nextPid));
    }

    return true;
}

bool Kernel::hasCycleDFS(int node, map<int, vector<int>>& graph,
                         set<int>& visited, set<int>& inStack,
                         vector<int>& cyclePath) {
    visited.insert(node);
    inStack.insert(node);
    cyclePath.push_back(node);

    if (graph.find(node) != graph.end()) {
        for (int neighbor : graph[node]) {
            if (inStack.count(neighbor)) {
                cyclePath.push_back(neighbor);
                return true;
            }
            if (!visited.count(neighbor)) {
                if (hasCycleDFS(neighbor, graph, visited, inStack, cyclePath))
                    return true;
            }
        }
    }

    inStack.erase(node);
    cyclePath.pop_back();
    return false;
}

bool Kernel::detectDeadlock() {
    // Build wait-for graph
    map<int, vector<int>> graph;
    set<int> allPids;

    for (auto& entry : resources) {
        auto& rl = entry.second;
        if (rl.heldBy != -1) {
            for (int waiter : rl.waiters) {
                graph[waiter].push_back(rl.heldBy);
                allPids.insert(waiter);
                allPids.insert(rl.heldBy);
            }
        }
    }

    // DFS cycle detection
    set<int> visited, inStack;
    for (int pid : allPids) {
        if (!visited.count(pid)) {
            vector<int> cyclePath;
            if (hasCycleDFS(pid, graph, visited, inStack, cyclePath)) {
                // Print the cycle
                OS_LockGuard lock(printMutex);
                cout << "  Deadlock cycle: ";
                for (size_t i = 0; i < cyclePath.size(); i++) {
                    cout << "PID " << cyclePath[i];
                    if (i < cyclePath.size() - 1) cout << " → ";
                }
                cout << "\n";
                return true;
            }
        }
    }

    return false;
}

void Kernel::printWaitForGraph() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Wait-For Graph ==========\n";
    bool hasEdges = false;

    for (auto& entry : resources) {
        auto& rl = entry.second;
        if (rl.heldBy != -1 && !rl.waiters.empty()) {
            for (int waiter : rl.waiters) {
                cout << "  PID " << waiter << " --waits-for('" << rl.name
                     << "')--> PID " << rl.heldBy << "\n";
                hasEdges = true;
            }
        }
    }

    if (!hasEdges) {
        cout << "  No waiting relationships. No deadlock possible.\n";
    }

    cout << "  ====================================\n\n";
}

void Kernel::printResourceTable() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Resource Table ==========\n";
    cout << "  +------------------+--------+-------------------+\n";
    cout << "  |    Resource      | Holder |     Waiters       |\n";
    cout << "  +------------------+--------+-------------------+\n";

    if (resources.empty()) {
        cout << "  |          No resources locked                  |\n";
    } else {
        for (auto& entry : resources) {
            auto& rl = entry.second;
            cout << "  | " << setw(16) << left << rl.name << right << " | ";

            if (rl.heldBy == -1) cout << "  FREE ";
            else cout << setw(6) << rl.heldBy;
            cout << " | ";

            if (rl.waiters.empty()) {
                cout << "     none          ";
            } else {
                string waitStr = "";
                for (int w : rl.waiters) {
                    if (!waitStr.empty()) waitStr += ", ";
                    waitStr += "PID " + to_string(w);
                }
                cout << setw(17) << left << waitStr << right;
            }
            cout << " |\n";
        }
    }

    cout << "  +------------------+--------+-------------------+\n";
    cout << "  ====================================\n\n";
}

// =====================================================
//  IPC CHANNELS
// =====================================================

void Kernel::createChannel(const string& name, int pid) {
    if (channels.find(name) != channels.end()) {
        OS_LockGuard lock(printMutex);
        cout << "[IPC] Channel '" << name << "' already exists.\n";
        return;
    }

    IPCChannel ch;
    ch.name = name;
    ch.ownerPid = pid;
    channels[name] = ch;

    OS_LockGuard lock(printMutex);
    cout << "[IPC] Channel '" << name << "' created by PID " << pid << "\n";
    sysLogger.log("[IPC]", "Channel '" + name + "' created by PID " + to_string(pid));
}

bool Kernel::sendToChannel(const string& name, const string& message) {
    if (channels.find(name) == channels.end()) {
        OS_LockGuard lock(printMutex);
        cout << "[IPC] Channel '" << name << "' does not exist.\n";
        return false;
    }

    channels[name].buffer.push(message);

    OS_LockGuard lock(printMutex);
    cout << "[IPC] Message sent to channel '" << name << "': \"" << message << "\"\n";
    sysLogger.log("[IPC]", "Message sent to '" + name + "': " + message);
    return true;
}

string Kernel::receiveFromChannel(const string& name) {
    if (channels.find(name) == channels.end()) {
        OS_LockGuard lock(printMutex);
        cout << "[IPC] Channel '" << name << "' does not exist.\n";
        return "";
    }

    if (channels[name].buffer.empty()) {
        OS_LockGuard lock(printMutex);
        cout << "[IPC] Channel '" << name << "' is empty — no messages.\n";
        return "";
    }

    string msg = channels[name].buffer.front();
    channels[name].buffer.pop();

    OS_LockGuard lock(printMutex);
    cout << "[IPC] Received from channel '" << name << "': \"" << msg << "\"\n";
    sysLogger.log("[IPC]", "Received from '" + name + "': " + msg);
    return msg;
}

void Kernel::listChannels() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== IPC Channels ==========\n";
    cout << "  +------------------+-------+-----------+\n";
    cout << "  |    Channel       | Owner | Buffered  |\n";
    cout << "  +------------------+-------+-----------+\n";

    if (channels.empty()) {
        cout << "  |         No channels created              |\n";
    } else {
        for (auto& entry : channels) {
            cout << "  | " << setw(16) << left << entry.second.name << right
                 << " | " << setw(5) << entry.second.ownerPid
                 << " | " << setw(9) << entry.second.buffer.size() << " |\n";
        }
    }

    cout << "  +------------------+-------+-----------+\n";
    cout << "  ====================================\n\n";
}

// =====================================================
//  BACKGROUND SCHEDULER THREAD
// =====================================================

#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI KernelSchedulerBody(LPVOID param) {
    Kernel* k = (Kernel*)param;
#else
void KernelSchedulerBody(Kernel* k) {
#endif
    while (k->isRunning()) {
        Message interruptMsg;
        interruptMsg.sender = 0;
        interruptMsg.type = "interrupt";
        interruptMsg.data = "timer";
        k->sendMessage(interruptMsg);

        k->processMessages();

        os_sleep_ms(1000);
    }
#if defined(_WIN32) || defined(_WIN64)
    return 0;
#endif
}

void Kernel::startScheduler() {
    if (running) return;
    running = true;
    schedulerThread.start(KernelSchedulerBody, this);
    sysLogger.log("[Kernel]", "Scheduler thread started");
}

void Kernel::stopScheduler() {
    running = false;
    if (schedulerThread.joinable()) {
        schedulerThread.join();
    }
    sysLogger.log("[Kernel]", "Scheduler thread stopped");
}

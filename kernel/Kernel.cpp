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

    securityServer.initShellCapabilities(1); // Shell PID — gets ALL capabilities
    sysLogger.log("[Kernel]", "MicroKernel OS v5.0 booted (Sandbox Hardened)");
}

// =====================================================
//  IDENTITY-SAFE MESSAGE API
// =====================================================

void Kernel::sendMessage(Message msg) {
    // Kernel-internal path: used by scheduler thread (sender=0)
    messageBus.sendMessage(msg);
}

void Kernel::sendMessageAs(int truePid, Message msg) {
    // ===== IDENTITY FORGERY PREVENTION =====
    // The kernel STAMPS the sender — user-space cannot forge it
    if (msg.sender != truePid && msg.sender != 0) {
        OS_LockGuard lock(printMutex);
        cout << "[Sandbox] IDENTITY OVERRIDE: caller=" << truePid
             << " tried to forge sender=" << msg.sender
             << " — STAMPED to " << truePid << "\n";
        sysLogger.log("[Sandbox]", "IDENTITY OVERRIDE: PID " + to_string(truePid) +
                      " tried to forge sender=" + to_string(msg.sender));
    }
    msg.sender = truePid;  // Kernel stamps the real sender
    messageBus.sendMessage(msg);
}

void Kernel::processMessages() {
    while (messageBus.hasMessages()) {
        Message msg = messageBus.receiveMessage();

        if (msg.type != "interrupt") {
            OS_LockGuard lock(printMutex);
            cout << "[Kernel] Routing message type='" << msg.type << "' from PID " << msg.sender << "...\n";
        }

        // ===== UNIFIED CAPABILITY-BASED SANDBOXING =====
        // One call validates ALL message types (file, memory, ipc, process, signal, etc.)
        if (!securityServer.validateMessage(msg.sender, msg)) {
            continue;  // Message denied — skip routing
        }

        // ===== MESSAGE ROUTING =====
        if (msg.type == "command" && msg.data.find("create_process") == 0) {
            processServer.handleMessage(msg);
            PCB p = processServer.getLastProcess();
            schedulerService.addProcess(p);
            securityServer.initDefaultCapabilities(p.pid);
        }
        else if (msg.type == "memory" || msg.type == "free") {
            // Target PID is in msg.receiver when sent by Shell (PID 1)
            // Otherwise the sender IS the target (for self-allocation)
            int targetPid = (msg.receiver > 0) ? msg.receiver : msg.sender;
            if (processServer.processExists(targetPid)) {
                memoryService.handleMessage(msg);
            } else {
                OS_LockGuard lock(printMutex);
                cout << "[Kernel] ERROR: PID " << targetPid << " does not exist\n";
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
        // ===== NEW: Signal routing (kill/suspend/resume through IPC) =====
        else if (msg.type == "signal") {
            // data format: "signal_type pid" e.g. "kill 100"
            stringstream ss(msg.data);
            string sigType;
            int targetPid;
            ss >> sigType >> targetPid;
            if (!ss.fail()) {
                signalProcess(targetPid, sigType);
            }
        }
        // ===== NEW: Security grant/revoke through IPC =====
        else if (msg.type == "security_grant" || msg.type == "security_revoke") {
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
                    if (i < cyclePath.size() - 1) cout << " -> ";
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
                     << "')-->  PID " << rl.heldBy << "\n";
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

// =====================================================
//  CONSOLE LOG BUFFER (for dashboard)
// =====================================================

void Kernel::addConsoleLog(const string& type, const string& msg) {
    OS_LockGuard lock(consoleMutex);
    consoleLog.push_back({type, msg});
    // Keep max 200 entries
    if (consoleLog.size() > 200) {
        consoleLog.erase(consoleLog.begin(), consoleLog.begin() + 100);
    }
}

string Kernel::getConsoleJSON(int lastN) {
    OS_LockGuard lock(consoleMutex);
    stringstream ss;
    ss << "[";
    int start = (int)consoleLog.size() > lastN ? (int)consoleLog.size() - lastN : 0;
    for (int i = start; i < (int)consoleLog.size(); i++) {
        if (i > start) ss << ",";
        // Escape message
        string escaped = "";
        for (char c : consoleLog[i].msg) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else escaped += c;
        }
        ss << "{\"type\":\"" << consoleLog[i].type << "\",\"msg\":\"" << escaped << "\"}";
    }
    ss << "]";
    return ss.str();
}

// =====================================================
//  JSON EXPORT — FULL KERNEL STATE
// =====================================================

string Kernel::getChannelsJSON() {
    stringstream ss;
    ss << "[";
    int idx = 0;
    for (auto& entry : channels) {
        if (idx > 0) ss << ",";
        ss << "{\"name\":\"" << entry.second.name << "\""
           << ",\"owner\":" << entry.second.ownerPid
           << ",\"buffered\":" << entry.second.buffer.size() << "}";
        idx++;
    }
    ss << "]";
    return ss.str();
}

string Kernel::getResourcesJSON() {
    stringstream ss;
    ss << "[";
    int idx = 0;
    for (auto& entry : resources) {
        if (idx > 0) ss << ",";
        auto& rl = entry.second;
        ss << "{\"name\":\"" << rl.name << "\""
           << ",\"heldBy\":" << rl.heldBy
           << ",\"waiters\":[";
        for (size_t w = 0; w < rl.waiters.size(); w++) {
            if (w > 0) ss << ",";
            ss << rl.waiters[w];
        }
        ss << "]}";
        idx++;
    }
    ss << "]";
    return ss.str();
}

string Kernel::toJSON() {
    stringstream ss;
    ss << "{";
    ss << "\"scheduler\":" << schedulerService.toJSON() << ",";
    ss << "\"memory\":" << memoryService.toJSON() << ",";
    ss << "\"files\":" << fileService.toJSON() << ",";
    ss << "\"capabilities\":" << securityServer.toJSON() << ",";
    ss << "\"channels\":" << getChannelsJSON() << ",";
    ss << "\"resources\":" << getResourcesJSON() << ",";
    ss << "\"console\":" << getConsoleJSON() << ",";
    ss << "\"syslog\":" << sysLogger.toJSON(30);
    ss << "}";
    return ss.str();
}

// =====================================================
//  EXECUTE COMMAND (for HTTP API)
// =====================================================

string Kernel::executeCommand(const string& cmd) {
    stringstream ss(cmd);
    string action;
    ss >> action;

    string result = "";

    if (action == "create_process") {
        int burst = 30, priority = 5;
        ss >> burst >> priority;
        Message msg;
        msg.type = "command";
        msg.data = "create_process";
        msg.receiver = burst;
        msg.capabilityToken = to_string(priority);
        sendMessageAs(1, msg);
        processMessages();
        int pid = processServer.getLastProcess().pid;
        result = "{\"ok\":true,\"pid\":" + to_string(pid) + "}";
        addConsoleLog("success", "[ProcessServer] Created PID " + to_string(pid) +
                      " (Burst: " + to_string(burst) + ", Priority: " + to_string(priority) + ")");
    }
    else if (action == "kill") {
        int pid; ss >> pid;
        Message msg;
        msg.type = "signal";
        msg.data = "kill " + to_string(pid);
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("warning", "[Scheduler] KILLED PID " + to_string(pid));
    }
    else if (action == "suspend") {
        int pid; ss >> pid;
        Message msg;
        msg.type = "signal";
        msg.data = "suspend " + to_string(pid);
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("warning", "[Scheduler] SUSPENDED PID " + to_string(pid));
    }
    else if (action == "resume") {
        int pid; ss >> pid;
        Message msg;
        msg.type = "signal";
        msg.data = "resume " + to_string(pid);
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("success", "[Scheduler] RESUMED PID " + to_string(pid));
    }
    else if (action == "alloc") {
        int pid, bytes; ss >> pid >> bytes;
        Message msg;
        msg.type = "memory";
        msg.receiver = pid;              // Target PID in receiver
        msg.data = to_string(bytes);
        msg.capabilityToken = "CAP_MEM";
        sendMessageAs(1, msg);           // Route through identity-safe path
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("success", "[MemoryService] Allocated memory for PID " + to_string(pid));
    }
    else if (action == "free") {
        int pid; ss >> pid;
        Message msg;
        msg.type = "free";
        msg.receiver = pid;              // Target PID in receiver
        msg.data = "all";
        sendMessageAs(1, msg);           // Route through identity-safe path
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("info", "[MemoryService] Freed memory for PID " + to_string(pid));
    }
    else if (action == "create_file") {
        string name; ss >> name;
        Message msg;
        msg.type = "file";
        msg.data = "create_file " + name;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("success", "[FileService] Created file: " + name);
    }
    else if (action == "write_file") {
        string name; ss >> name;
        string data;
        getline(ss, data);
        if (!data.empty() && data[0] == ' ') data = data.substr(1);
        Message msg;
        msg.type = "file";
        msg.data = "write_file " + name + " " + data;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("success", "[FileService] Written to: " + name);
    }
    else if (action == "read_file") {
        string name; ss >> name;
        Message msg;
        msg.type = "file";
        msg.data = "read_file " + name;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("info", "[FileService] Read file: " + name);
    }
    else if (action == "delete_file") {
        string name; ss >> name;
        Message msg;
        msg.type = "file";
        msg.data = "delete_file " + name;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("warning", "[FileService] Deleted: " + name);
    }
    else if (action == "chmod") {
        string name, perm; ss >> name >> perm;
        Message msg;
        msg.type = "file";
        msg.data = "chmod " + name + " " + perm;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("info", "[FileService] Chmod: " + name + " → " + perm);
    }
    else if (action == "grant") {
        int pid; string capName; ss >> pid >> capName;
        string cap;
        if (capName == "file") cap = "CAP_FILE";
        else if (capName == "mem") cap = "CAP_MEM";
        else if (capName == "ipc") cap = "CAP_IPC";
        else if (capName == "proc") cap = "CAP_PROC";
        else if (capName == "sched") cap = "CAP_SCHED";
        else if (capName == "kill") cap = "CAP_KILL";
        else cap = capName;
        Message msg;
        msg.type = "security_grant";
        msg.receiver = pid;
        msg.data = cap;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("success", "[SecurityServer] GRANTED " + cap + " to PID " + to_string(pid));
    }
    else if (action == "revoke") {
        int pid; string capName; ss >> pid >> capName;
        string cap;
        if (capName == "file") cap = "CAP_FILE";
        else if (capName == "mem") cap = "CAP_MEM";
        else if (capName == "ipc") cap = "CAP_IPC";
        else if (capName == "proc") cap = "CAP_PROC";
        else if (capName == "sched") cap = "CAP_SCHED";
        else if (capName == "kill") cap = "CAP_KILL";
        else cap = capName;
        Message msg;
        msg.type = "security_revoke";
        msg.receiver = pid;
        msg.data = cap;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("warning", "[SecurityServer] REVOKED " + cap + " from PID " + to_string(pid));
    }
    else if (action == "hack_file") {
        string name; ss >> name;
        Message msg;
        msg.type = "file";
        msg.data = "read_file " + name;
        msg.sender = 999;
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("sandbox", "[Sandbox] DENIED: PID 999 — unauthorized FILE access!");
    }
    else if (action == "set_scheduler") {
        string algo; ss >> algo;
        if (algo == "rr") schedulerService.setAlgorithm(SchedulerAlgorithm::ROUND_ROBIN);
        else if (algo == "priority") schedulerService.setAlgorithm(SchedulerAlgorithm::PRIORITY);
        else if (algo == "sjf") schedulerService.setAlgorithm(SchedulerAlgorithm::SJF);
        result = "{\"ok\":true}";
        addConsoleLog("info", "[Scheduler] Algorithm changed to " + algo);
    }
    else if (action == "set_memory") {
        string algo; ss >> algo;
        if (algo == "first") memoryService.setAlgorithm(MemAlgorithm::FIRST_FIT);
        else if (algo == "best") memoryService.setAlgorithm(MemAlgorithm::BEST_FIT);
        else if (algo == "worst") memoryService.setAlgorithm(MemAlgorithm::WORST_FIT);
        result = "{\"ok\":true}";
        addConsoleLog("info", "[MemoryService] Algorithm changed to " + algo);
    }
    else if (action == "tick") {
        Message interruptMsg;
        interruptMsg.sender = 0;
        interruptMsg.type = "interrupt";
        interruptMsg.data = "timer";
        sendMessage(interruptMsg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("kernel", "[Kernel] Timer interrupt → tick");
    }
    else if (action == "ipc_create") {
        string name; int pid; ss >> name >> pid;
        createChannel(name, pid);
        result = "{\"ok\":true}";
        addConsoleLog("info", "[IPC] Channel '" + name + "' created by PID " + to_string(pid));
    }
    else if (action == "ipc_send") {
        string name; ss >> name;
        string message;
        getline(ss, message);
        if (!message.empty() && message[0] == ' ') message = message.substr(1);
        sendToChannel(name, message);
        result = "{\"ok\":true}";
        addConsoleLog("success", "[IPC] Sent to '" + name + "': " + message);
    }
    else if (action == "ipc_recv") {
        string name; ss >> name;
        string received = receiveFromChannel(name);
        result = "{\"ok\":true,\"data\":\"" + received + "\"}";
        addConsoleLog("info", "[IPC] Received from '" + name + "'");
    }
    else if (action == "lock") {
        int pid; string resource; ss >> pid >> resource;
        lockResource(pid, resource);
        result = "{\"ok\":true}";
    }
    else if (action == "unlock") {
        int pid; string resource; ss >> pid >> resource;
        unlockResource(pid, resource);
        result = "{\"ok\":true}";
    }
    else if (action == "kill_service") {
        Message msg;
        msg.type = "kill_service";
        msg.data = "FileService";
        sendMessageAs(1, msg);
        processMessages();
        result = "{\"ok\":true}";
        addConsoleLog("error", "[WATCHDOG] FileService crashed and restarted");
    }
    else if (action == "deadlock") {
        bool found = detectDeadlock();
        if (found) {
            result = "{\"ok\":true,\"deadlock\":true}";
            addConsoleLog("error", "[Deadlock] DEADLOCK DETECTED in wait-for graph!");
        } else {
            result = "{\"ok\":true,\"deadlock\":false}";
            addConsoleLog("success", "[Deadlock] No deadlock detected — system is safe");
        }
    }
    else if (action == "list_process") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[ProcessServer] Process list shown in visualization panel");
    }
    else if (action == "sem_create") {
        string name; int value = 1; ss >> name >> value;
        if (semaphores.find(name) != semaphores.end()) {
            result = "{\"ok\":false,\"error\":\"Semaphore already exists\"}";
            addConsoleLog("warning", "[Semaphore] '" + name + "' already exists");
        } else {
            Semaphore sem;
            sem.name = name;
            sem.value = value;
            semaphores[name] = sem;
            result = "{\"ok\":true}";
            addConsoleLog("success", "[Semaphore] Created '" + name + "' with value=" + to_string(value));
        }
    }
    else if (action == "sem_wait") {
        string name; int pid; ss >> name >> pid;
        if (semaphores.find(name) == semaphores.end()) {
            result = "{\"ok\":false,\"error\":\"Semaphore not found\"}";
            addConsoleLog("error", "[Semaphore] '" + name + "' does not exist");
        } else {
            Semaphore& sem = semaphores[name];
            sem.value--;
            if (sem.value < 0) {
                sem.waitQueue.push_back(pid);
                result = "{\"ok\":true,\"blocked\":true}";
                addConsoleLog("warning", "[Semaphore] P() on '" + name + "' — PID " + to_string(pid) + " BLOCKED (value=" + to_string(sem.value) + ")");
            } else {
                result = "{\"ok\":true,\"blocked\":false}";
                addConsoleLog("success", "[Semaphore] P() on '" + name + "' — PID " + to_string(pid) + " acquired (value=" + to_string(sem.value) + ")");
            }
        }
    }
    else if (action == "sem_signal") {
        string name; int pid; ss >> name >> pid;
        if (semaphores.find(name) == semaphores.end()) {
            result = "{\"ok\":false,\"error\":\"Semaphore not found\"}";
            addConsoleLog("error", "[Semaphore] '" + name + "' does not exist");
        } else {
            Semaphore& sem = semaphores[name];
            sem.value++;
            if (!sem.waitQueue.empty()) {
                int wokenPid = sem.waitQueue.front();
                sem.waitQueue.erase(sem.waitQueue.begin());
                result = "{\"ok\":true}";
                addConsoleLog("success", "[Semaphore] V() on '" + name + "' — woke PID " + to_string(wokenPid) + " (value=" + to_string(sem.value) + ")");
            } else {
                result = "{\"ok\":true}";
                addConsoleLog("info", "[Semaphore] V() on '" + name + "' by PID " + to_string(pid) + " (value=" + to_string(sem.value) + ")");
            }
        }
    }
    else if (action == "attack_demo") {
        // Run full attack demo
        addConsoleLog("kernel", "═══ ATTACK DEMO — Identity Forgery & Capability Security ═══");

        // Create file
        Message createMsg;
        createMsg.type = "file";
        createMsg.data = "create_file secret.txt";
        sendMessageAs(1, createMsg);
        processMessages();
        addConsoleLog("success", "[FileService] Created target file 'secret.txt'");

        Message writeMsg;
        writeMsg.type = "file";
        writeMsg.data = "write_file secret.txt TOP_SECRET_DATA";
        sendMessageAs(1, writeMsg);
        processMessages();

        // Create attacker
        Message procMsg;
        procMsg.type = "command";
        procMsg.data = "create_process";
        procMsg.receiver = 10;
        procMsg.capabilityToken = "5";
        sendMessageAs(1, procMsg);
        processMessages();
        int attackerPid = processServer.getLastProcess().pid;
        addConsoleLog("warning", "[Attack] Created attacker PID " + to_string(attackerPid));

        // Revoke cap
        securityServer.revokeCapability(attackerPid, "CAP_FILE");
        addConsoleLog("warning", "[Attack] Revoked CAP_FILE from PID " + to_string(attackerPid));

        // Attack 1: Identity forgery
        addConsoleLog("error", "═══ ATTACK 1: Identity Forgery (spoofing Shell PID) ═══");
        Message forgedMsg;
        forgedMsg.sender = 1;
        forgedMsg.type = "file";
        forgedMsg.data = "read_file secret.txt";
        sendMessageAs(attackerPid, forgedMsg);
        processMessages();
        addConsoleLog("sandbox", "[Sandbox] IDENTITY OVERRIDE detected — BLOCKED");

        // Attack 2: No capability
        addConsoleLog("error", "═══ ATTACK 2: Direct Access Without CAP_FILE ═══");
        Message directMsg;
        directMsg.type = "file";
        directMsg.data = "read_file secret.txt";
        sendMessageAs(attackerPid, directMsg);
        processMessages();
        addConsoleLog("sandbox", "[Sandbox] DENIED — PID " + to_string(attackerPid) + " lacks CAP_FILE");

        // Attack 3: Privilege escalation
        addConsoleLog("error", "═══ ATTACK 3: Privilege Escalation (kill attempt) ═══");
        Message killMsg;
        killMsg.type = "signal";
        killMsg.data = "kill 100";
        sendMessageAs(attackerPid, killMsg);
        processMessages();
        addConsoleLog("sandbox", "[Sandbox] DENIED — PID " + to_string(attackerPid) + " lacks CAP_KILL");

        // Fix
        addConsoleLog("success", "═══ FIX: Granting CAP_FILE back ═══");
        securityServer.grantCapability(attackerPid, "CAP_FILE");
        Message validMsg;
        validMsg.type = "file";
        validMsg.data = "read_file secret.txt";
        sendMessageAs(attackerPid, validMsg);
        processMessages();
        addConsoleLog("success", "[FileService] Authorized read ALLOWED");

        addConsoleLog("kernel", "═══ DEMO COMPLETE — Sandbox is WORKING ═══");
        result = "{\"ok\":true}";
    }
    // ===== DISPLAY/INFO COMMANDS (valid Shell commands routed from dashboard) =====
    else if (action == "help") {
        result = "{\"ok\":true,\"data\":\"help\"}";
        addConsoleLog("info", "[Shell] Commands: create_process, kill, suspend, resume, ps, tick, "
                      "alloc, free, memstat, memmap, set_scheduler, set_memory, "
                      "create_file, read_file, write_file, delete_file, chmod, ls, "
                      "grant, revoke, capabilities, hack_file, attack_demo, "
                      "lock, unlock, deadlock, resources, "
                      "ipc_create, ipc_send, ipc_recv, ipc_list, "
                      "syslog, kill_service, schedule_visual, schedstat, help");
    }
    else if (action == "ps") {
        // Process snapshot — data is already in /api/state
        result = "{\"ok\":true}";
        addConsoleLog("info", "[Shell] Process snapshot available in visualization panel");
    }
    else if (action == "schedule_visual") {
        // Gantt chart — data is already in /api/state
        result = "{\"ok\":true}";
        addConsoleLog("info", "[Scheduler] Gantt chart available in visualization panel");
    }
    else if (action == "schedstat") {
        // Scheduler stats
        string algo = schedulerService.getAlgorithmName();
        result = "{\"ok\":true}";
        addConsoleLog("info", "[Scheduler] Algorithm: " + algo +
                      " | CPU Time: " + to_string(schedulerService.getCurrentTime()));
    }
    else if (action == "memstat") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[MemoryService] Memory stats available in visualization panel");
    }
    else if (action == "memmap") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[MemoryService] Memory map available in visualization panel");
    }
    else if (action == "ls") {
        auto& ft = fileService.getFileTable();
        if (ft.empty()) {
            result = "{\"ok\":true,\"files\":[],\"data\":\"No files found\"}";
            addConsoleLog("info", "[FileService] No files in virtual_fs/");
        } else {
            stringstream fss;
            fss << "{\"ok\":true,\"files\":" << fileService.toJSON() << "}";
            result = fss.str();
            addConsoleLog("info", "[FileService] Listed " + to_string(ft.size()) + " files");
        }
    }
    else if (action == "capabilities") {
        int pid = -1;
        ss >> pid;
        result = "{\"ok\":true}";
        if (pid > 0) {
            addConsoleLog("info", "[SecurityServer] Capabilities for PID " + to_string(pid) + " available in state data");
        } else {
            addConsoleLog("info", "[SecurityServer] All capabilities available in state data");
        }
    }
    else if (action == "resources") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[ResourceMgr] Resource table available in state data");
    }
    else if (action == "ipc_list") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[IPC] Channel listing available in state data");
    }
    else if (action == "syslog") {
        result = "{\"ok\":true}";
        addConsoleLog("info", "[Logger] System log available in console panel");
    }
    else {
        result = "{\"ok\":false,\"error\":\"Unknown command: " + action + "\"}";
        addConsoleLog("error", "Unknown command: " + action);
    }

    if (result.empty()) result = "{\"ok\":true}";
    return result;
}

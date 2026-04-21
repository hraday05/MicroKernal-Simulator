#ifndef KERNEL_H
#define KERNEL_H

#include <queue>
#include <map>
#include <set>
#include <vector>
#include <string>
#include "../ipc/IPC.h"
#include "../services/ProcessServer.h"
#include "../services/MemoryService.h"
#include "../services/FileService.h"
#include "../kernel/OS_Thread.h"
#include <atomic>
#include "../kernel/OS_Mutex.h"
#include "../services/SchedulerService.h"
#include "../services/SecurityServer.h"

// Resource lock for deadlock detection
struct ResourceLock {
    std::string name;
    int heldBy;                    // PID holding it, -1 if free
    std::vector<int> waiters;      // PIDs waiting for this resource
};

// IPC Channel for inter-process communication
struct IPCChannel {
    std::string name;
    int ownerPid;
    std::queue<std::string> buffer;
};

// Console log entry for dashboard
struct ConsoleEntry {
    std::string type;   // "info", "success", "error", "kernel", "sandbox", "warning"
    std::string msg;
};

class Kernel {
private:
    MessageBus messageBus;
    ProcessServer processServer;
    MemoryService memoryService;
    FileService fileService;
    SchedulerService schedulerService;
    SecurityServer securityServer;

    OS_Thread schedulerThread;
    std::atomic<bool> running;

    // Resource management (deadlock detection)
    std::map<std::string, ResourceLock> resources;

    // IPC Channels
    std::map<std::string, IPCChannel> channels;

    // Console log buffer (for dashboard)
    std::vector<ConsoleEntry> consoleLog;
    OS_Mutex consoleMutex;

    // Deadlock detection helpers
    bool hasCycleDFS(int node, std::map<int, std::vector<int>>& graph,
                     std::set<int>& visited, std::set<int>& inStack,
                     std::vector<int>& cyclePath);

public:
    Kernel();

    // === IDENTITY-SAFE MESSAGE API ===
    void sendMessage(Message msg);
    void sendMessageAs(int truePid, Message msg);
    void processMessages();

    void startScheduler();
    void stopScheduler();

    bool isRunning() { return running; }

    // Expose services (read-only display operations)
    ProcessServer& getProcessServer() { return processServer; }
    SchedulerService& getSchedulerService() { return schedulerService; }
    MemoryService& getMemoryService() { return memoryService; }
    SecurityServer& getSecurityServer() { return securityServer; }
    FileService& getFileService() { return fileService; }

    // Process signals
    void signalProcess(int pid, const std::string& signal);

    // Resource locking + deadlock detection
    bool lockResource(int pid, const std::string& resource);
    bool unlockResource(int pid, const std::string& resource);
    bool detectDeadlock();
    void printWaitForGraph();
    void printResourceTable();

    // IPC Channels
    void createChannel(const std::string& name, int pid);
    bool sendToChannel(const std::string& name, const std::string& message);
    std::string receiveFromChannel(const std::string& name);
    void listChannels();

    // === HTTP API SUPPORT ===
    std::string toJSON();                                    // Full state as JSON
    std::string executeCommand(const std::string& cmd);      // Execute command, return JSON result
    void addConsoleLog(const std::string& type, const std::string& msg);
    std::string getConsoleJSON(int lastN = 50);
    std::string getChannelsJSON();
    std::string getResourcesJSON();
};

#endif

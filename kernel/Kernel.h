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

    // Deadlock detection helpers
    bool hasCycleDFS(int node, std::map<int, std::vector<int>>& graph,
                     std::set<int>& visited, std::set<int>& inStack,
                     std::vector<int>& cyclePath);

public:
    Kernel();

    void sendMessage(Message msg);
    void processMessages();

    void startScheduler();
    void stopScheduler();

    bool isRunning() { return running; }

    // Expose services
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
};

#endif

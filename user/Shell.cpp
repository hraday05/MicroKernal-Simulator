#include "Shell.h"
#include "../kernel/Globals.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/OS_Thread.h"
#include "../kernel/Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;

Shell::Shell(Kernel *k) { kernel = k; }

void Shell::printHelp() {
    OS_LockGuard lock(printMutex);
    cout << "\n==================== MicroKernel OS v5.0 - Help ====================\n";
    cout << "  PROCESS MANAGEMENT:\n";
    cout << "    create_process [burst] [priority]  - Create process (burst=30, pri=5)\n";
    cout << "    list_process                       - List all active processes\n";
    cout << "    ps                                 - Show scheduler snapshot\n";
    cout << "    kill <pid>                         - Terminate a process\n";
    cout << "    suspend <pid>                      - Pause a process (BLOCKED)\n";
    cout << "    resume <pid>                       - Resume a paused process\n";
    cout << "\n  SCHEDULING:\n";
    cout << "    schedule_visual                    - Show Gantt chart + log\n";
    cout << "    schedstat                          - Show scheduler statistics\n";
    cout << "    set_scheduler <rr|priority|sjf>    - Change scheduling algorithm\n";
    cout << "\n  MEMORY MANAGEMENT:\n";
    cout << "    alloc <pid> <bytes>                - Allocate memory for a process\n";
    cout << "    free <pid>                         - Free all memory for a process\n";
    cout << "    memstat                            - Show memory statistics\n";
    cout << "    memmap                             - Show visual memory map\n";
    cout << "    set_memory <first|best|worst>      - Change allocation algorithm\n";
    cout << "\n  FILE SYSTEM (Persistent):\n";
    cout << "    create_file <name>                 - Create a persistent file\n";
    cout << "    read_file <name>                   - Read a file\n";
    cout << "    write_file <name> <data>           - Write data to a file\n";
    cout << "    delete_file <name>                 - Delete a file\n";
    cout << "    chmod <name> <read|write|both|none>- Change file permissions\n";
    cout << "    ls                                 - List all files\n";
    cout << "\n  SECURITY:\n";
    cout << "    grant <pid> <file|mem>             - Grant capability to a process\n";
    cout << "    revoke <pid> <file|mem>            - Revoke capability from a process\n";
    cout << "    capabilities [pid]                 - Show capability table\n";
    cout << "    hack_file <name>                   - Attempt unauthorized file access\n";
    cout << "\n  CONCURRENCY & IPC:\n";
    cout << "    lock <pid> <resource>              - Lock a resource (for deadlock demo)\n";
    cout << "    unlock <pid> <resource>            - Unlock a resource\n";
    cout << "    deadlock                           - Check for deadlocks\n";
    cout << "    resources                          - Show resource lock table\n";
    cout << "    ipc_create <channel> <pid>         - Create an IPC channel\n";
    cout << "    ipc_send <channel> <message>       - Send message to channel\n";
    cout << "    ipc_recv <channel>                 - Receive message from channel\n";
    cout << "    ipc_list                           - List all IPC channels\n";
    cout << "\n  SYSTEM:\n";
    cout << "    syslog [n]                         - View last N system log entries\n";
    cout << "    kill_service                       - Simulate FileService crash\n";
    cout << "    help                               - Show this help menu\n";
    cout << "    exit                               - Exit the simulator\n";
    cout << "=====================================================================\n\n";
}

void Shell::run() {
  string command;

  {
    OS_LockGuard lock(printMutex);
    cout << "\n";
    cout << "==============================================\n";
    cout << "   MicroKernel OS Simulator v5.0\n";
    cout << "   Scheduler: Round Robin (Quantum = 5)\n";
    cout << "   Memory:    First Fit\n";
    cout << "   File System: Persistent (virtual_fs/)\n";
    cout << "   Security: Capability-Based Sandboxing\n";
    cout << "   Concurrency: Signals, IPC, Deadlock Detect\n";
    cout << "   Type 'help' for available commands\n";
    cout << "==============================================\n\n";
  }

  while (true) {
    os_sleep_ms(50);
    {
      OS_LockGuard lock(printMutex);
      cout << ">> " << flush;
    }
    getline(cin, command);

    if (command.empty()) continue;
    if (command == "exit") break;
    if (command == "help") { printHelp(); continue; }

    // ===========================================================
    //  DIRECT COMMANDS (no IPC message needed)
    // ===========================================================

    // --- ps ---
    if (command == "ps") {
      auto& scheduler = kernel->getSchedulerService();
      auto snapshot = scheduler.getProcessSnapshot();

      OS_LockGuard lock(printMutex);
      cout << "\n  Algorithm: " << scheduler.getAlgorithmName() << " (Quantum = 5)\n\n";
      cout << "+-------+--------------------+--------+-----------+----------+---------+\n";
      cout << "|  PID  |       Name         | Burst  | Remaining | Priority |  State  |\n";
      cout << "+-------+--------------------+--------+-----------+----------+---------+\n";

      if (snapshot.empty()) {
        cout << "|                   No processes in scheduler                         |\n";
      } else {
        for (auto& p : snapshot) {
          cout << "| " << setw(5) << p.pid << " | "
               << setw(18) << left << p.name << right << " | "
               << setw(6) << p.burstTime << " | "
               << setw(9) << p.remainingTime << " | "
               << setw(8) << p.priority << " | "
               << setw(7) << p.state << " |\n";
        }
      }
      cout << "+-------+--------------------+--------+-----------+----------+---------+\n\n";
      continue;
    }

    // --- schedule_visual ---
    if (command == "schedule_visual") {
      kernel->getSchedulerService().printGanttChart();
      kernel->getSchedulerService().printDetailedLog();
      continue;
    }
    if (command == "schedstat") {
      kernel->getSchedulerService().printStats();
      continue;
    }

    // --- set_scheduler ---
    if (command.find("set_scheduler") == 0) {
      stringstream ss(command); string cmd, algo; ss >> cmd >> algo;
      if (algo == "rr")       kernel->getSchedulerService().setAlgorithm(SchedulerAlgorithm::ROUND_ROBIN);
      else if (algo == "priority") kernel->getSchedulerService().setAlgorithm(SchedulerAlgorithm::PRIORITY);
      else if (algo == "sjf")      kernel->getSchedulerService().setAlgorithm(SchedulerAlgorithm::SJF);
      else cout << "Usage: set_scheduler <rr|priority|sjf>\n";
      continue;
    }

    // --- memstat / memmap ---
    if (command == "memstat") { kernel->getMemoryService().printStats(); continue; }
    if (command == "memmap")  { kernel->getMemoryService().printMemoryMap(); continue; }

    // --- set_memory ---
    if (command.find("set_memory") == 0) {
      stringstream ss(command); string cmd, algo; ss >> cmd >> algo;
      if (algo == "first" || algo == "first_fit")     kernel->getMemoryService().setAlgorithm(MemAlgorithm::FIRST_FIT);
      else if (algo == "best"  || algo == "best_fit")  kernel->getMemoryService().setAlgorithm(MemAlgorithm::BEST_FIT);
      else if (algo == "worst" || algo == "worst_fit") kernel->getMemoryService().setAlgorithm(MemAlgorithm::WORST_FIT);
      else cout << "Usage: set_memory <first|best|worst>\n";
      continue;
    }

    // --- ls ---
    if (command == "ls") { kernel->getFileService().listFiles(); continue; }

    // --- capabilities ---
    if (command.find("capabilities") == 0) {
      stringstream ss(command); string cmd; int pid = -1; ss >> cmd;
      if (ss >> pid) kernel->getSecurityServer().printCapabilities(pid);
      else kernel->getSecurityServer().printAllCapabilities();
      continue;
    }

    // --- grant ---
    if (command.find("grant") == 0) {
      stringstream ss(command); string cmd, capName; int pid; ss >> cmd >> pid >> capName;
      if (ss.fail()) { cout << "Usage: grant <pid> <file|mem>\n"; continue; }
      string cap;
      if (capName == "file") cap = "CAP_FILE";
      else if (capName == "mem" || capName == "memory") cap = "CAP_MEM";
      else { cout << "Unknown capability: " << capName << "\n"; continue; }
      kernel->getSecurityServer().grantCapability(pid, cap);
      continue;
    }

    // --- revoke ---
    if (command.find("revoke") == 0) {
      stringstream ss(command); string cmd, capName; int pid; ss >> cmd >> pid >> capName;
      if (ss.fail()) { cout << "Usage: revoke <pid> <file|mem>\n"; continue; }
      string cap;
      if (capName == "file") cap = "CAP_FILE";
      else if (capName == "mem" || capName == "memory") cap = "CAP_MEM";
      else { cout << "Unknown capability: " << capName << "\n"; continue; }
      kernel->getSecurityServer().revokeCapability(pid, cap);
      continue;
    }

    // --- PROCESS SIGNALS ---
    if (command.find("kill ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: kill <pid>\n"; continue; }
      kernel->signalProcess(pid, "kill");
      kernel->processMessages();  // process cleanup messages
      continue;
    }

    if (command.find("suspend ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: suspend <pid>\n"; continue; }
      kernel->signalProcess(pid, "suspend");
      continue;
    }

    if (command.find("resume ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: resume <pid>\n"; continue; }
      kernel->signalProcess(pid, "resume");
      continue;
    }

    // --- RESOURCE LOCKING ---
    if (command.find("lock ") == 0) {
      stringstream ss(command); string cmd, resource; int pid;
      ss >> cmd >> pid >> resource;
      if (ss.fail()) { cout << "Usage: lock <pid> <resource>\n"; continue; }
      kernel->lockResource(pid, resource);
      continue;
    }

    if (command.find("unlock ") == 0) {
      stringstream ss(command); string cmd, resource; int pid;
      ss >> cmd >> pid >> resource;
      if (ss.fail()) { cout << "Usage: unlock <pid> <resource>\n"; continue; }
      kernel->unlockResource(pid, resource);
      continue;
    }

    if (command == "deadlock") {
      if (!kernel->detectDeadlock()) {
        OS_LockGuard lock(printMutex);
        cout << "\n  No deadlock detected. System is safe.\n\n";
      }
      kernel->printWaitForGraph();
      continue;
    }

    if (command == "resources") {
      kernel->printResourceTable();
      continue;
    }

    // --- IPC CHANNELS ---
    if (command.find("ipc_create") == 0) {
      stringstream ss(command); string cmd, name; int pid;
      ss >> cmd >> name >> pid;
      if (ss.fail()) { cout << "Usage: ipc_create <channel> <pid>\n"; continue; }
      kernel->createChannel(name, pid);
      continue;
    }

    if (command.find("ipc_send") == 0) {
      stringstream ss(command); string cmd, name;
      ss >> cmd >> name;
      string message;
      getline(ss, message);
      size_t firstNonSpace = message.find_first_not_of(" ");
      if (firstNonSpace != string::npos) message = message.substr(firstNonSpace);
      if (name.empty() || message.empty()) { cout << "Usage: ipc_send <channel> <message>\n"; continue; }
      kernel->sendToChannel(name, message);
      continue;
    }

    if (command.find("ipc_recv") == 0) {
      stringstream ss(command); string cmd, name;
      ss >> cmd >> name;
      if (name.empty()) { cout << "Usage: ipc_recv <channel>\n"; continue; }
      kernel->receiveFromChannel(name);
      continue;
    }

    if (command == "ipc_list") {
      kernel->listChannels();
      continue;
    }

    // --- SYSTEM LOG ---
    if (command.find("syslog") == 0) {
      stringstream ss(command); string cmd; int n = 20;
      ss >> cmd;
      if (ss >> n) {} // use provided number
      sysLogger.printLog(n);
      continue;
    }

    // ===========================================================
    //  COMMANDS THAT GO THROUGH KERNEL IPC
    // ===========================================================

    Message msg;
    msg.sender = 1;
    msg.receiver = 0;

    // --- create_process [burst] [priority] ---
    if (command.find("create_process") == 0) {
      msg.type = "command";
      msg.data = "create_process";
      stringstream ss(command); string cmd; int burst = 0, priority = 0;
      ss >> cmd;
      if (ss >> burst) {
        msg.receiver = burst;
        if (ss >> priority) msg.capabilityToken = to_string(priority);
      }
    }

    // --- alloc <pid> <bytes> ---
    else if (command.find("alloc") == 0) {
      msg.type = "memory";
      msg.capabilityToken = "CAP_MEM";
      stringstream ss(command); string cmd; int pid, amount;
      ss >> cmd >> pid >> amount;
      if (ss.fail()) { cout << "Usage: alloc <pid> <bytes>\n"; continue; }
      msg.data = to_string(amount);
      msg.sender = pid;
    }

    // --- free <pid> ---
    else if (command.find("free") == 0) {
      msg.type = "free";
      stringstream ss(command); string cmd; int pid;
      ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: free <pid>\n"; continue; }
      msg.sender = pid;
      msg.data = "all";
    }

    // --- hack_file ---
    else if (command.find("hack_file") == 0) {
      stringstream ss(command); string cmd, filename; ss >> cmd >> filename;
      if (filename.empty()) { cout << "Usage: hack_file <filename>\n"; continue; }
      msg.type = "file";
      msg.data = "read_file " + filename;
      msg.sender = 999;
      msg.capabilityToken = "";
    }

    // --- chmod ---
    else if (command.find("chmod") == 0) {
      msg.type = "file";
      msg.data = command;
      msg.capabilityToken = "CAP_FILE";
    }

    // --- file operations ---
    else if (command.find("create_file") == 0 ||
             command.find("read_file") == 0 ||
             command.find("delete_file") == 0 ||
             command.find("write_file") == 0) {
      msg.type = "file";
      msg.data = command;
      msg.capabilityToken = "CAP_FILE";
    }

    // --- kill_service ---
    else if (command.find("kill_service") == 0) {
      msg.type = "kill_service";
      msg.data = "FileService";
    }

    // --- everything else ---
    else {
      msg.type = "command";
      msg.data = command;
    }

    kernel->sendMessage(msg);
    kernel->processMessages();
  }
}

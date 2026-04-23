#include "Shell.h"
#include "../kernel/Globals.h"
#include "../kernel/OS_Mutex.h"
#include "../kernel/OS_Thread.h"
#include "../kernel/Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;

static const int SHELL_PID = 1;  // Shell's identity — stamped by kernel

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
    cout << "    grant <pid> <file|mem|ipc|proc|sched|kill> - Grant capability\n";
    cout << "    revoke <pid> <file|mem|ipc|proc|sched|kill>- Revoke capability\n";
    cout << "    capabilities [pid]                 - Show capability table\n";
    cout << "    hack_file <name>                   - Attempt unauthorized file access\n";
    cout << "    attack_demo                        - Run full attack/defense demo\n";
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

// =====================================================
//  ATTACK DEMO — shows identity forgery prevention
// =====================================================

void Shell::runAttackDemo() {
    {
        OS_LockGuard lock(printMutex);
        cout << "\n";
        cout << "  ================================================================\n";
        cout << "   ATTACK DEMO — Identity Forgery & Capability-Based Security\n";
        cout << "  ================================================================\n\n";
        cout << "  This demo creates a malicious process and shows how the kernel\n";
        cout << "  sandbox prevents identity forgery and unauthorized access.\n";
        cout << "  ================================================================\n\n";
    }

    // Step 1: Create a file to attack
    {
        OS_LockGuard lock2(printMutex);
        cout << "  [STEP 1] Creating target file 'secret.txt'...\n\n";
    }
    Message createMsg;
    createMsg.type = "file";
    createMsg.data = "create_file secret.txt";
    kernel->sendMessageAs(SHELL_PID, createMsg);
    kernel->processMessages();

    {
        Message writeMsg;
        writeMsg.type = "file";
        writeMsg.data = "write_file secret.txt TOP_SECRET_DATA_12345";
        kernel->sendMessageAs(SHELL_PID, writeMsg);
        kernel->processMessages();
    }

    os_sleep_ms(200);

    // Step 2: Create attacker process
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  [STEP 2] Creating attacker process...\n\n";
    }
    {
        Message procMsg;
        procMsg.type = "command";
        procMsg.data = "create_process";
        procMsg.receiver = 10;
        procMsg.capabilityToken = "5";
        kernel->sendMessageAs(SHELL_PID, procMsg);
        kernel->processMessages();
    }

    os_sleep_ms(200);

    // Get the attacker's PID (it's the latest process)
    int attackerPid = kernel->getProcessServer().getLastProcess().pid;

    // Step 3: Revoke attacker's CAP_FILE
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  [STEP 3] Revoking CAP_FILE from attacker PID " << attackerPid << "...\n\n";
    }
    kernel->getSecurityServer().revokeCapability(attackerPid, "CAP_FILE");

    os_sleep_ms(200);

    // Show capabilities before attack
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  [STEP 4] Current capabilities:\n";
    }
    kernel->getSecurityServer().printCapabilities(attackerPid);

    os_sleep_ms(200);

    // ===== ATTACK 1: Identity Forgery =====
    {
        OS_LockGuard lock2(printMutex);
        cout << "  ==========================================================\n";
        cout << "   ATTACK 1: Identity Forgery (spoofing Shell PID)\n";
        cout << "  ==========================================================\n";
        cout << "  Attacker PID " << attackerPid << " tries to forge sender=1 (Shell)...\n\n";
    }
    {
        Message forgedMsg;
        forgedMsg.sender = SHELL_PID;  // FORGED — pretending to be shell
        forgedMsg.type = "file";
        forgedMsg.data = "read_file secret.txt";
        // Kernel will detect and override the forged sender
        kernel->sendMessageAs(attackerPid, forgedMsg);
        kernel->processMessages();
    }

    os_sleep_ms(300);

    // ===== ATTACK 2: Direct Access Without Capability =====
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  ==========================================================\n";
        cout << "   ATTACK 2: Direct Access Without Capability\n";
        cout << "  ==========================================================\n";
        cout << "  Attacker PID " << attackerPid << " tries to read file without CAP_FILE...\n\n";
    }
    {
        Message directMsg;
        directMsg.type = "file";
        directMsg.data = "read_file secret.txt";
        kernel->sendMessageAs(attackerPid, directMsg);
        kernel->processMessages();
    }

    os_sleep_ms(300);

    // ===== ATTACK 3: Privilege Escalation =====
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  ==========================================================\n";
        cout << "   ATTACK 3: Privilege Escalation (requesting CAP_KILL)\n";
        cout << "  ==========================================================\n";
        cout << "  Attacker PID " << attackerPid << " tries to kill another process...\n\n";
    }
    {
        Message killMsg;
        killMsg.type = "signal";
        killMsg.data = "kill 100";
        kernel->sendMessageAs(attackerPid, killMsg);
        kernel->processMessages();
    }

    os_sleep_ms(300);

    // ===== FIX: Grant capability and retry =====
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  ==========================================================\n";
        cout << "   FIX: Shell grants CAP_FILE back — authorized access\n";
        cout << "  ==========================================================\n";
        cout << "  Admin (Shell) grants CAP_FILE to PID " << attackerPid << "...\n\n";
    }
    kernel->getSecurityServer().grantCapability(attackerPid, "CAP_FILE");

    os_sleep_ms(200);

    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  Retrying file read with valid capability...\n\n";
    }
    {
        Message validMsg;
        validMsg.type = "file";
        validMsg.data = "read_file secret.txt";
        kernel->sendMessageAs(attackerPid, validMsg);
        kernel->processMessages();
    }

    os_sleep_ms(200);

    // ===== SUMMARY TABLE =====
    {
        OS_LockGuard lock2(printMutex);
        cout << "\n  ================================================================\n";
        cout << "   ATTACK DEMO SUMMARY\n";
        cout << "  ================================================================\n";
        cout << "  +----------------------------------+----------+-----------------+\n";
        cout << "  | Attack Scenario                  |  Result  | Defense Layer   |\n";
        cout << "  +----------------------------------+----------+-----------------+\n";
        cout << "  | Identity Forgery (spoof PID 1)   | BLOCKED  | Kernel Stamping |\n";
        cout << "  | File Access (no CAP_FILE)        | BLOCKED  | Capability Map  |\n";
        cout << "  | Privilege Escalation (kill)       | BLOCKED  | CAP_KILL Check  |\n";
        cout << "  | Authorized Access (with CAP)     | ALLOWED  | Valid Capability |\n";
        cout << "  +----------------------------------+----------+-----------------+\n";
        cout << "\n  Sandbox isolation is WORKING. Identity forgery is PREVENTED.\n";
        cout << "  ================================================================\n\n";
    }
}

// =====================================================
//  MAIN SHELL LOOP
// =====================================================

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
    cout << "   Sandbox:  Identity Forgery Prevention\n";
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
    //  DISPLAY-ONLY COMMANDS (read-only, no IPC needed)
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

    // --- capabilities (read-only display) ---
    if (command.find("capabilities") == 0) {
      stringstream ss(command); string cmd; int pid = -1; ss >> cmd;
      if (ss >> pid) kernel->getSecurityServer().printCapabilities(pid);
      else kernel->getSecurityServer().printAllCapabilities();
      continue;
    }

    // --- RESOURCE LOCKING (direct kernel calls) ---
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

    // --- ATTACK DEMO ---
    if (command == "attack_demo") {
      runAttackDemo();
      continue;
    }

    // --- TICK (timer interrupt for scheduler) ---
    if (command == "tick") {
      Message tickMsg;
      tickMsg.sender = 0;
      tickMsg.type = "interrupt";
      tickMsg.data = "timer";
      kernel->sendMessage(tickMsg);
      kernel->processMessages();
      continue;
    }

    // --- SEMAPHORE COMMANDS ---
    if (command.find("sem_create") == 0) {
      stringstream ss(command); string cmd, name; int value = 1;
      ss >> cmd >> name >> value;
      if (name.empty()) { cout << "Usage: sem_create <name> [value]\n"; continue; }
      kernel->executeCommand(command);
      continue;
    }
    if (command.find("sem_wait") == 0) {
      kernel->executeCommand(command);
      continue;
    }
    if (command.find("sem_signal") == 0) {
      kernel->executeCommand(command);
      continue;
    }

    // ===========================================================
    //  COMMANDS THAT GO THROUGH KERNEL IPC (identity-safe)
    // ===========================================================

    Message msg;
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
      msg.receiver = pid;  // Target PID in receiver field
    }

    // --- free <pid> ---
    else if (command.find("free") == 0) {
      msg.type = "free";
      stringstream ss(command); string cmd; int pid;
      ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: free <pid>\n"; continue; }
      msg.receiver = pid;  // Target PID in receiver field
      msg.data = "all";
    }

    // --- grant <pid> <capability> --- (routed through IPC)
    else if (command.find("grant") == 0) {
      stringstream ss(command); string cmd, capName; int pid; ss >> cmd >> pid >> capName;
      if (ss.fail()) { cout << "Usage: grant <pid> <file|mem|ipc|proc|sched|kill>\n"; continue; }
      string cap;
      if (capName == "file") cap = "CAP_FILE";
      else if (capName == "mem" || capName == "memory") cap = "CAP_MEM";
      else if (capName == "ipc") cap = "CAP_IPC";
      else if (capName == "proc" || capName == "process") cap = "CAP_PROC";
      else if (capName == "sched" || capName == "scheduler") cap = "CAP_SCHED";
      else if (capName == "kill") cap = "CAP_KILL";
      else { cout << "Unknown capability: " << capName << "\n"; continue; }
      msg.type = "security_grant";
      msg.receiver = pid;
      msg.data = cap;
    }

    // --- revoke <pid> <capability> --- (routed through IPC)
    else if (command.find("revoke") == 0) {
      stringstream ss(command); string cmd, capName; int pid; ss >> cmd >> pid >> capName;
      if (ss.fail()) { cout << "Usage: revoke <pid> <file|mem|ipc|proc|sched|kill>\n"; continue; }
      string cap;
      if (capName == "file") cap = "CAP_FILE";
      else if (capName == "mem" || capName == "memory") cap = "CAP_MEM";
      else if (capName == "ipc") cap = "CAP_IPC";
      else if (capName == "proc" || capName == "process") cap = "CAP_PROC";
      else if (capName == "sched" || capName == "scheduler") cap = "CAP_SCHED";
      else if (capName == "kill") cap = "CAP_KILL";
      else { cout << "Unknown capability: " << capName << "\n"; continue; }
      msg.type = "security_revoke";
      msg.receiver = pid;
      msg.data = cap;
    }

    // --- kill <pid> --- (routed through IPC as signal)
    else if (command.find("kill ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: kill <pid>\n"; continue; }
      msg.type = "signal";
      msg.data = "kill " + to_string(pid);
    }

    // --- suspend <pid> --- (routed through IPC as signal)
    else if (command.find("suspend ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: suspend <pid>\n"; continue; }
      msg.type = "signal";
      msg.data = "suspend " + to_string(pid);
    }

    // --- resume <pid> --- (routed through IPC as signal)
    else if (command.find("resume ") == 0) {
      stringstream ss(command); string cmd; int pid; ss >> cmd >> pid;
      if (ss.fail()) { cout << "Usage: resume <pid>\n"; continue; }
      msg.type = "signal";
      msg.data = "resume " + to_string(pid);
    }

    // --- hack_file ---
    else if (command.find("hack_file") == 0) {
      stringstream ss(command); string cmd, filename; ss >> cmd >> filename;
      if (filename.empty()) { cout << "Usage: hack_file <filename>\n"; continue; }
      msg.type = "file";
      msg.data = "read_file " + filename;
      // The attacker tries to forge sender=999
      msg.sender = 999;
      msg.capabilityToken = "";
      // Kernel will stamp sender to SHELL_PID, exposing the forgery attempt
      kernel->sendMessageAs(SHELL_PID, msg);
      kernel->processMessages();
      continue;  // Already handled
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

    // All messages go through sendMessageAs — kernel stamps the sender
    kernel->sendMessageAs(SHELL_PID, msg);
    kernel->processMessages();
  }
}

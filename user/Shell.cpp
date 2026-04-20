#include "Shell.h"
#include "../kernel/Globals.h"
#include "../kernel/OS_Mutex.h"
#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;

Shell::Shell(Kernel *k) { kernel = k; }

void Shell::printHelp() {
    OS_LockGuard lock(printMutex);
    cout << "\n========== MicroKernel OS - Help ==========\n";
    cout << "  create_process          - Create a new process\n";
    cout << "  list_process            - List all processes\n";
    cout << "  ps                      - Show running/ready processes with status\n";
    cout << "  alloc <pid> <bytes>     - Allocate memory for a process\n";
    cout << "  free <bytes> <pid>      - Free memory for a process\n";
    cout << "  create_file <name>      - Create a virtual file\n";
    cout << "  read_file <name>        - Read a virtual file\n";
    cout << "  write_file <name> <data>- Write data to a virtual file\n";
    cout << "  delete_file <name>      - Delete a virtual file\n";
    cout << "  kill_service            - Simulate a FileService crash + watchdog recovery\n";
    cout << "  memstat                 - Show memory usage statistics\n";
    cout << "  help                    - Show this help menu\n";
    cout << "  exit                    - Exit the simulator\n";
    cout << "=============================================\n\n";
}

void Shell::run() {
  string command;

  // Print welcome banner
  {
    OS_LockGuard lock(printMutex);
    cout << "\n";
    cout << "==============================================\n";
    cout << "   MicroKernel OS Simulator v2.0\n";
    cout << "   Scheduler: Round Robin (Quantum = 2)\n";
    cout << "   Type 'help' for available commands\n";
    cout << "==============================================\n\n";
  }

  while (true) {
    // Wait a tiny moment so any background output finishes first
    Sleep(50);
    {
      OS_LockGuard lock(printMutex);
      cout << ">> " << flush;
    }
    getline(cin, command);

    if (command.empty()) continue;
    if (command == "exit") break;

    if (command == "help") {
      printHelp();
      continue;
    }

    // ps command - shows currently running/ready processes
    if (command == "ps") {
      auto& scheduler = kernel->getSchedulerService();
      auto snapshot = scheduler.getProcessSnapshot();

      OS_LockGuard lock(printMutex);
      cout << "\n  Scheduling Algorithm: Round Robin (Quantum = 2)\n\n";
      cout << "+-------+--------------------+--------+---------+\n";
      cout << "|  PID  |       Name         | Burst  |  State  |\n";
      cout << "+-------+--------------------+--------+---------+\n";

      if (snapshot.empty()) {
        cout << "|            No processes in scheduler          |\n";
      } else {
        for (auto& p : snapshot) {
          cout << "| " << setw(5) << p.pid << " | "
               << setw(18) << left << p.name << right << " | "
               << setw(6) << p.burstTime << " | "
               << setw(7) << p.state << " |\n";
        }
      }
      cout << "+-------+--------------------+--------+---------+\n\n";
      continue;
    }

    // memstat command
    if (command == "memstat") {
      auto& mem = kernel->getMemoryService();
      mem.printStats();
      continue;
    }

    Message msg;
    msg.sender = 1;   // Shell PID
    msg.receiver = 0; // Kernel
    if (command.find("alloc") == 0) {
      msg.type = "memory";
      msg.capabilityToken = "CAP_MEM";

      stringstream ss(command);
      string cmd;
      int amount, pid;

      ss >> cmd >> pid >> amount;

      if (ss.fail()) {
        cout << "Usage: alloc <pid> <bytes>\n";
        continue;
      }

      msg.data = to_string(amount);
      msg.sender = pid;
    }

    else if (command.find("free") == 0) {
      msg.type = "free";

      stringstream ss(command);
      string cmd;
      int amount, pid;

      ss >> cmd >> amount >> pid;

      if (ss.fail()) {
        cout << "Usage: free <bytes> <pid>\n";
        continue;
      }

      msg.data = to_string(amount);
      msg.sender = pid;
    } else if (command.find("create_file") == 0 ||
               command.find("read_file") == 0 ||
               command.find("delete_file") == 0 ||
               command.find("write_file") == 0) {
      msg.type = "file";
      msg.data = command;
      msg.capabilityToken = "CAP_FILE";
    } else if (command.find("kill_service") == 0) {
      msg.type = "kill_service";
      msg.data = "FileService";
    } else {
      msg.type = "command";
      msg.data = command;
    }
    kernel->sendMessage(msg);
    kernel->processMessages();
  }
}

#include "Logger.h"
#include "Globals.h"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

// Global system logger instance
Logger sysLogger;

Logger::Logger() {
#if defined(_WIN32) || defined(_WIN64)
    startTime = GetTickCount();
#else
    startTime = chrono::steady_clock::now();
#endif
}

void Logger::log(const string& component, const string& message) {
    OS_LockGuard lock(mutex);
    LogEntry entry;

#if defined(_WIN32) || defined(_WIN64)
    entry.timestamp = GetTickCount() - startTime;
#else
    auto now = chrono::steady_clock::now();
    entry.timestamp = (unsigned long)chrono::duration_cast<chrono::milliseconds>(now - startTime).count();
#endif

    entry.component = component;
    entry.message = message;
    entries.push_back(entry);
}

string Logger::formatTimestamp(TimeStamp ts) {
    int ms = ts % 1000;
    int sec = (ts / 1000) % 60;
    int min = (ts / 60000) % 60;
    stringstream ss;
    ss << setfill('0') << setw(2) << min << ":"
       << setw(2) << sec << "."
       << setw(3) << ms;
    return ss.str();
}

int Logger::getEntryCount() {
    OS_LockGuard lock(mutex);
    return (int)entries.size();
}

void Logger::printLog(int lastN) {
    OS_LockGuard lock(mutex);
    OS_LockGuard plock(printMutex);

    cout << "\n  ==================== System Log ====================\n";

    if (entries.empty()) {
        cout << "  No log entries yet.\n";
    } else {
        int start = (int)entries.size() > lastN ? (int)entries.size() - lastN : 0;
        for (int i = start; i < (int)entries.size(); i++) {
            cout << "  [" << formatTimestamp(entries[i].timestamp) << "] "
                 << entries[i].component << " " << entries[i].message << "\n";
        }
    }

    cout << "  Total entries: " << entries.size() << "\n";
    cout << "  =====================================================\n\n";
}

#ifndef LOGGER_H
#define LOGGER_H

// Cross-platform logger
// Windows: uses GetTickCount()
// macOS/Linux: uses C++11 chrono::steady_clock

#include <string>
#include <vector>
#include "OS_Mutex.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    typedef DWORD TimeStamp;
#else
    #include <chrono>
    typedef unsigned long TimeStamp;
#endif

struct LogEntry {
    TimeStamp timestamp;       // ms since boot
    std::string component;     // [Kernel], [FileService], etc.
    std::string message;
};

class Logger {
private:
    std::vector<LogEntry> entries;
    OS_Mutex mutex;

#if defined(_WIN32) || defined(_WIN64)
    DWORD startTime;
#else
    std::chrono::steady_clock::time_point startTime;
#endif

public:
    Logger();
    void log(const std::string& component, const std::string& message);
    void printLog(int lastN = 20);
    std::string formatTimestamp(TimeStamp ts);
    int getEntryCount();

    // JSON export for HTTP API
    std::string toJSON(int lastN = 50);
};

// Global system logger
extern Logger sysLogger;

#endif

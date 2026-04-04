#ifndef OS_THREAD_H
#define OS_THREAD_H

#include <windows.h>

// A very lightweight abstraction over Windows native threading API
// This completely avoids the OS_Thread compiler limitation in MinGW GCC 6.3!
class OS_Thread {
private:
    HANDLE handle;
    DWORD threadId;

public:
    OS_Thread() : handle(NULL), threadId(0) {}
    
    ~OS_Thread() {
        join(); // Ensure thread cleanup on destruction
    }

    void start(LPTHREAD_START_ROUTINE func, LPVOID param) {
        handle = CreateThread(
            NULL,       // default security attributes
            0,          // default stack size
            func,       // thread function name
            param,      // argument to thread function
            0,          // default creation flags
            &threadId); // returns the thread identifier
    }

    void join() {
        if (handle) {
            WaitForSingleObject(handle, INFINITE);
            CloseHandle(handle);
            handle = NULL;
        }
    }

    bool joinable() {
        return handle != NULL;
    }
};

#endif


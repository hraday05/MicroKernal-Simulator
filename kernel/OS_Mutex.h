#ifndef OS_MUTEX_H
#define OS_MUTEX_H

#include <windows.h>

class OS_Mutex {
private:
    CRITICAL_SECTION cs;
public:
    OS_Mutex() {
        InitializeCriticalSection(&cs);
    }
    
    ~OS_Mutex() {
        DeleteCriticalSection(&cs);
    }
    
    void lock() {
        EnterCriticalSection(&cs);
    }
    
    void unlock() {
        LeaveCriticalSection(&cs);
    }
};

class OS_LockGuard {
private:
    OS_Mutex& mtx;
public:
    OS_LockGuard(OS_Mutex& m) : mtx(m) {
        mtx.lock();
    }
    
    ~OS_LockGuard() {
        mtx.unlock();
    }
};

#endif


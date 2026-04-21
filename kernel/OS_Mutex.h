#ifndef OS_MUTEX_H
#define OS_MUTEX_H

// Cross-platform mutex abstraction
// Windows: uses CRITICAL_SECTION (works with all MinGW versions)
// macOS/Linux: uses C++11 std::mutex

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    class OS_Mutex {
    private:
        CRITICAL_SECTION cs;
    public:
        OS_Mutex() { InitializeCriticalSection(&cs); }
        ~OS_Mutex() { DeleteCriticalSection(&cs); }
        void lock() { EnterCriticalSection(&cs); }
        void unlock() { LeaveCriticalSection(&cs); }
    };

#else
    #include <mutex>

    class OS_Mutex {
    private:
        std::mutex mtx;
    public:
        OS_Mutex() {}
        ~OS_Mutex() {}
        void lock() { mtx.lock(); }
        void unlock() { mtx.unlock(); }
    };

#endif

// RAII lock guard — works on both platforms
class OS_LockGuard {
private:
    OS_Mutex& mtx;
public:
    OS_LockGuard(OS_Mutex& m) : mtx(m) { mtx.lock(); }
    ~OS_LockGuard() { mtx.unlock(); }
};

#endif

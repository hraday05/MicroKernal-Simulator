#ifndef OS_THREAD_H
#define OS_THREAD_H

// Cross-platform threading abstraction
// Windows: uses native CreateThread (works with all MinGW versions)
// macOS/Linux: uses C++11 std::thread

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    class OS_Thread {
    private:
        HANDLE handle;
        DWORD threadId;
    public:
        OS_Thread() : handle(NULL), threadId(0) {}

        ~OS_Thread() { join(); }

        void start(LPTHREAD_START_ROUTINE func, LPVOID param) {
            handle = CreateThread(NULL, 0, func, param, 0, &threadId);
        }

        void join() {
            if (handle) {
                WaitForSingleObject(handle, INFINITE);
                CloseHandle(handle);
                handle = NULL;
            }
        }

        bool joinable() { return handle != NULL; }
    };

    // Cross-platform sleep
    inline void os_sleep_ms(int ms) { Sleep(ms); }

#else
    #include <thread>
    #include <chrono>
    #include <functional>

    class OS_Thread {
    private:
        std::thread t;
    public:
        OS_Thread() {}

        ~OS_Thread() { join(); }

        // Generic start for any callable
        template<typename Func, typename... Args>
        void start(Func&& func, Args&&... args) {
            if (t.joinable()) t.join();
            t = std::thread(std::forward<Func>(func), std::forward<Args>(args)...);
        }

        void join() {
            if (t.joinable()) t.join();
        }

        bool joinable() { return t.joinable(); }
    };

    // Cross-platform sleep
    inline void os_sleep_ms(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

#endif

#endif

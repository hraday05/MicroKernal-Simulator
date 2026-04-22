# ============================================
# MicroKernel OS Simulator v5.0
# Cross-platform Makefile (macOS / Linux / Windows-MinGW)
# ============================================

CXX = g++
CXXFLAGS = -std=c++14 -Wall -pthread
TARGET = microkernel

# Detect Windows for Winsock
ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32
    TARGET = microkernel.exe
endif

# Source files
SRCS = main.cpp \
       kernel/Kernel.cpp \
       kernel/Logger.cpp \
       kernel/Globals.cpp \
       ipc/IPC.cpp \
       services/ProcessServer.cpp \
       services/SchedulerService.cpp \
       services/MemoryService.cpp \
       services/FileService.cpp \
       services/SecurityServer.cpp \
       server/HttpServer.cpp \
       user/Shell.cpp

# Default target
all: $(TARGET)
	@echo ""
	@echo "✅ Build successful! Run with: ./$(TARGET)"
	@echo ""

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Run the simulator
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all run clean

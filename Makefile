# ============================================
# MicroKernel OS Simulator v5.0
# Cross-platform Makefile (macOS / Linux)
# ============================================

CXX = g++
CXXFLAGS = -std=c++14 -Wall -pthread
TARGET = microkernel

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
       user/Shell.cpp

# Default target
all: $(TARGET)
	@echo ""
	@echo "✅ Build successful! Run with: ./$(TARGET)"
	@echo ""

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# Run the simulator
run: $(TARGET)
	./$(TARGET)

# Run the dashboard
dashboard:
	@echo "Starting dashboard at http://localhost:8080"
	@cd dashboard && python3 -m http.server 8080

# Clean build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all run dashboard clean

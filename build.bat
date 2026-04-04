@echo off
echo Compiling Microkernel Simulator...
g++ -std=c++14 -pthread main.cpp kernel\*.cpp ipc\*.cpp services\*.cpp user\*.cpp -o simulator.exe
if %ERRORLEVEL% == 0 (
    echo Compilation Successful!
    echo Running Simulator...
    simulator.exe
) else (
    echo Compilation Failed!
)
pause

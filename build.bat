@echo off
echo Compiling Microkernel Simulator v5.0...
g++ -std=c++14 main.cpp kernel\*.cpp ipc\*.cpp services\*.cpp user\*.cpp server\*.cpp -o microkernel.exe -lws2_32
if %ERRORLEVEL% == 0 (
    echo Compilation Successful!
    echo Running Simulator...
    microkernel.exe
) else (
    echo Compilation Failed!
)
pause

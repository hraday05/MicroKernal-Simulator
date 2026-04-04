#ifndef FILESERVICE_H
#define FILESERVICE_H

#include "../ipc/IPC.h"
#include <unordered_map>
#include <string>
#include "../kernel/OS_Mutex.h"
#include "Service.h"

class FileService : public Service {
private:
    std::unordered_map<std::string, std::string> files;

public:
    FileService();
    void handleMessage(Message msg);
};

#endif


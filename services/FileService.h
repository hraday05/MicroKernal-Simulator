#ifndef FILESERVICE_H
#define FILESERVICE_H

#include "../ipc/IPC.h"
#include <string>
#include "../kernel/OS_Mutex.h"
#include "Service.h"

class FileService : public Service {
private:
    std::string fsDir;                    // directory for persistent storage
    std::string getPath(const std::string& name);  // builds full path
    bool fileExists(const std::string& path);

public:
    FileService();
    void handleMessage(Message msg);
};

#endif

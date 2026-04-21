#ifndef FILESERVICE_H
#define FILESERVICE_H

#include "../ipc/IPC.h"
#include <string>
#include <map>
#include "../kernel/OS_Mutex.h"
#include "Service.h"

struct VirtualFile {
    std::string name;
    int ownerPid;       // PID that created this file
    bool readable;
    bool writable;
    bool isOpen;
};

class FileService : public Service {
private:
    std::string fsDir;
    std::map<std::string, VirtualFile> fileTable;  // runtime metadata

    std::string getPath(const std::string& name);
    bool fileExistsOnDisk(const std::string& path);
    void scanDirectory();  // loads existing files on startup

public:
    FileService();
    void handleMessage(Message msg);
    void listFiles();

    // JSON export for HTTP API
    std::string toJSON();
    std::map<std::string, VirtualFile>& getFileTable() { return fileTable; }
};

#endif

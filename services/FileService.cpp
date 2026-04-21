#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include "FileService.h"
#include "../kernel/Globals.h"

// Cross-platform directory handling
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <direct.h>
    #define MKDIR(dir) _mkdir(dir)
    #define PATH_SEP "\\"
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #define MKDIR(dir) mkdir(dir, 0755)
    #define PATH_SEP "/"
#endif

using namespace std;

static const string FS_DIR = "virtual_fs";

FileService::FileService() {
    fsDir = FS_DIR;

    // Cross-platform directory creation
    MKDIR(fsDir.c_str());

    scanDirectory();  // load existing files on startup
}

string FileService::getPath(const string& name) {
    return fsDir + PATH_SEP + name;
}

bool FileService::fileExistsOnDisk(const string& path) {
    ifstream f(path);
    return f.good();
}

// Cross-platform directory scanning
void FileService::scanDirectory() {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: use FindFirstFile/FindNextFile
    string pattern = fsDir + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        string name = findData.cFileName;
        if (name == "." || name == "..") continue;

        VirtualFile vf;
        vf.name = name;
        vf.ownerPid = 0;
        vf.readable = true;
        vf.writable = true;
        vf.isOpen = false;
        fileTable[name] = vf;
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);
#else
    // macOS / Linux: use opendir/readdir
    DIR* dir = opendir(fsDir.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name == "." || name == "..") continue;

        VirtualFile vf;
        vf.name = name;
        vf.ownerPid = 0;
        vf.readable = true;
        vf.writable = true;
        vf.isOpen = false;
        fileTable[name] = vf;
    }
    closedir(dir);
#endif
}

void FileService::handleMessage(Message msg) {
    if (msg.type != "file") return;

    stringstream ss(msg.data);
    string command, filename;
    ss >> command >> filename;

    if (command == "create_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (fileTable.find(filename) != fileTable.end()) {
            cout << "[FileService] Error: File '" << filename << "' already exists.\n";
        } else {
            ofstream out(path);
            out.close();

            VirtualFile vf;
            vf.name = filename;
            vf.ownerPid = msg.sender;
            vf.readable = true;
            vf.writable = true;
            vf.isOpen = false;
            fileTable[filename] = vf;

            cout << "[FileService] Created file: " << filename
                 << " (owner: PID " << msg.sender << ")\n";
        }
    }
    else if (command == "read_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (fileTable.find(filename) == fileTable.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        }
        else if (!fileTable[filename].readable) {
            cout << "[FileService] DENIED: File '" << filename << "' is not readable.\n";
        }
        else {
            ifstream in(path);
            string content = "";
            string line;
            while (getline(in, line)) {
                if (!content.empty()) content += "\n";
                content += line;
            }
            in.close();

            cout << "[FileService] Reading '" << filename << "':\n"
                 << content << "\n[EOF]\n";
        }
    }
    else if (command == "write_file") {
        string dataLine;
        getline(ss, dataLine);
        size_t firstNonSpace = dataLine.find_first_not_of(" ");
        if (firstNonSpace != string::npos) {
            dataLine = dataLine.substr(firstNonSpace);
        } else {
            dataLine = "";
        }

        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (fileTable.find(filename) == fileTable.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        }
        else if (!fileTable[filename].writable) {
            cout << "[FileService] DENIED: File '" << filename << "' is not writable.\n";
        }
        else {
            ofstream out(path);
            out << dataLine;
            out.close();
            cout << "[FileService] Written to file: " << filename << "\n";
        }
    }
    else if (command == "delete_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (fileTable.find(filename) == fileTable.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            remove(path.c_str());
            fileTable.erase(filename);
            cout << "[FileService] Deleted file: " << filename << "\n";
        }
    }
    else if (command == "chmod") {
        // chmod <filename> <read|write|both|none>
        string perm;
        ss >> perm;

        OS_LockGuard lock(printMutex);
        if (fileTable.find(filename) == fileTable.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            VirtualFile& vf = fileTable[filename];
            if (perm == "read")       { vf.readable = true;  vf.writable = false; }
            else if (perm == "write") { vf.readable = false; vf.writable = true;  }
            else if (perm == "both")  { vf.readable = true;  vf.writable = true;  }
            else if (perm == "none")  { vf.readable = false; vf.writable = false; }
            else {
                cout << "[FileService] Usage: chmod <file> <read|write|both|none>\n";
                return;
            }
            cout << "[FileService] Permissions changed for '" << filename
                 << "': R=" << (vf.readable ? "yes" : "no")
                 << " W=" << (vf.writable ? "yes" : "no") << "\n";
        }
    }
    else {
        OS_LockGuard lock(printMutex);
        cout << "[FileService] Unknown command: " << command << endl;
    }
}

void FileService::listFiles() {
    OS_LockGuard lock(printMutex);

    cout << "\n  ========== Virtual File System (virtual_fs/) ==========\n";
    cout << "  +--------------------+-------+------+------+-------+\n";
    cout << "  |     Filename       | Owner | Read | Write| Size  |\n";
    cout << "  +--------------------+-------+------+------+-------+\n";

    if (fileTable.empty()) {
        cout << "  |              No files found                      |\n";
    } else {
        for (auto& entry : fileTable) {
            string path = getPath(entry.first);
            // Get file size
            ifstream f(path, ios::ate);
            int size = f.is_open() ? (int)f.tellg() : 0;
            f.close();

            cout << "  | " << setw(18) << left << entry.first << right
                 << " | " << setw(5) << entry.second.ownerPid
                 << " | " << setw(4) << (entry.second.readable ? "YES" : "NO")
                 << " | " << setw(4) << (entry.second.writable ? "YES" : "NO")
                 << " | " << setw(4) << size << "B |\n";
        }
    }

    cout << "  +--------------------+-------+------+------+-------+\n";
    cout << "  =====================================================\n\n";
}

// =====================================================
//  JSON EXPORT FOR HTTP API
// =====================================================

string FileService::toJSON() {
    stringstream ss;
    ss << "[";
    int idx = 0;
    for (auto& entry : fileTable) {
        // Get file size
        string path = getPath(entry.first);
        ifstream f(path, ios::ate);
        int size = f.is_open() ? (int)f.tellg() : 0;
        f.close();

        if (idx > 0) ss << ",";
        ss << "{\"name\":\"" << entry.first << "\""
           << ",\"owner\":" << entry.second.ownerPid
           << ",\"readable\":" << (entry.second.readable ? "true" : "false")
           << ",\"writable\":" << (entry.second.writable ? "true" : "false")
           << ",\"size\":" << size << "}";
        idx++;
    }
    ss << "]";
    return ss.str();
}

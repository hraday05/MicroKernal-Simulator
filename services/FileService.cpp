#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include "FileService.h"
#include "../kernel/Globals.h"

using namespace std;

// Persistent storage directory (created next to the executable)
static const string FS_DIR = "virtual_fs";

FileService::FileService() {
    fsDir = FS_DIR;
    // Create the virtual_fs directory if it doesn't exist (Windows API)
    CreateDirectoryA(fsDir.c_str(), NULL);
}

string FileService::getPath(const string& name) {
    return fsDir + "\\" + name;
}

bool FileService::fileExists(const string& path) {
    ifstream f(path);
    return f.good();
}

void FileService::handleMessage(Message msg) {
    if (msg.type != "file") return;

    stringstream ss(msg.data);
    string command, filename;
    ss >> command >> filename;

    if (command == "create_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (fileExists(path)) {
            cout << "[FileService] Error: File '" << filename << "' already exists.\n";
        } else {
            ofstream out(path);
            out.close();
            cout << "[FileService] Created file: " << filename
                 << " (persistent: " << path << ")\n";
        }
    }
    else if (command == "read_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (!fileExists(path)) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
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
        // Remove leading spaces
        size_t firstNonSpace = dataLine.find_first_not_of(" ");
        if (firstNonSpace != string::npos) {
            dataLine = dataLine.substr(firstNonSpace);
        } else {
            dataLine = "";
        }

        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (!fileExists(path)) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            ofstream out(path);
            out << dataLine;
            out.close();
            cout << "[FileService] Written to file: " << filename << "\n";
        }
    }
    else if (command == "delete_file") {
        OS_LockGuard lock(printMutex);
        string path = getPath(filename);

        if (!fileExists(path)) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            remove(path.c_str());
            cout << "[FileService] Deleted file: " << filename << "\n";
        }
    }
    else {
        OS_LockGuard lock(printMutex);
        cout << "[FileService] Unknown command: " << command << endl;
    }
}

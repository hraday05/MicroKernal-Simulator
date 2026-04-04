#include <iostream>
#include <sstream>
#include "FileService.h"
#include "../kernel/Globals.h"

using namespace std;

FileService::FileService() {
    // Basic initialization if needed
}

void FileService::handleMessage(Message msg) {
    if (msg.type != "file") return;

    stringstream ss(msg.data);
    string command, filename;
    ss >> command >> filename;

    if (command == "create_file") {
        OS_LockGuard lock(printMutex);
        if (files.find(filename) != files.end()) {
            cout << "[FileService] Error: File '" << filename << "' already exists.\n";
        } else {
            files[filename] = "";
            cout << "[FileService] Created file: " << filename << endl;
        }
    } 
    else if (command == "read_file") {
        OS_LockGuard lock(printMutex);
        if (files.find(filename) == files.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            cout << "[FileService] Reading '" << filename << "': \n" << files[filename] << "\n[EOF]\n";
        }
    } 
    else if (command == "write_file") {
        string dataLine;
        getline(ss, dataLine);
        // remove leading spaces
        size_t firstNonSpace = dataLine.find_first_not_of(" ");
        if (firstNonSpace != string::npos) {
            dataLine = dataLine.substr(firstNonSpace);
        } else {
            dataLine = "";
        }

        OS_LockGuard lock(printMutex);
        if (files.find(filename) == files.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            files[filename] = dataLine;
            cout << "[FileService] Written to file: " << filename << endl;
        }
    } 
    else if (command == "delete_file") {
        OS_LockGuard lock(printMutex);
        if (files.find(filename) == files.end()) {
            cout << "[FileService] Error: File '" << filename << "' does not exist.\n";
        } else {
            files.erase(filename);
            cout << "[FileService] Deleted file: " << filename << endl;
        }
    }
    else {
        OS_LockGuard lock(printMutex);
        cout << "[FileService] Unknown command: " << command << endl;
    }
}


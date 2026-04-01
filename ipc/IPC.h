#ifndef IPC_H
#define IPC_H

#include <string>

struct Message {
    int sender;
    int receiver;
    std::string type;
    std::string data;
};

#endif
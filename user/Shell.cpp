#include <iostream>
#include "Shell.h"

using namespace std;

Shell::Shell(Kernel* k) {
    kernel = k;
}

void Shell::run() {
    string command;

    while (true) {
        cout << ">> ";
        cin >> command;

        if (command == "exit") break;

        kernel->sendMessage(command);
    }
}
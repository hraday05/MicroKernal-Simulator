#ifndef SHELL_H
#define SHELL_H

#include "../kernel/Kernel.h"
#include "../kernel/Globals.h"

class Shell {
private:
    Kernel* kernel;

public:
    Shell(Kernel* k);
    void run();
};

#endif

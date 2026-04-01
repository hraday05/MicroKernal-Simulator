#ifndef SHELL_H
#define SHELL_H

#include "../kernel/Kernel.h"

class Shell {
private:
    Kernel* kernel;

public:
    Shell(Kernel* k);
    void run();
};

#endif
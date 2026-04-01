#include <iostream>
#include "kernel/Kernel.h"
#include "user/Shell.h"

using namespace std;

int main() {
    Kernel kernel;
    Shell shell(&kernel);

    cout << "MicroKernel OS Started...\n";

    shell.run();

    return 0;
}
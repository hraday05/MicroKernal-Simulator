#include <iostream>
#include "Kernel.h"

using namespace std;

void Kernel::sendMessage(string msg) {
    cout << "[Kernel] Message received: " << msg << endl;
}
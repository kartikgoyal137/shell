#include "shell.h"
#include <iostream>

int main(int argc, char* /*argv*/[]) {
    // Unbuffered I/O
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Non-interactive mode: read from file/pipe
    if (argc > 1) {
        std::cerr << "ember: script execution not yet supported" << std::endl;
        return 1;
    }

    Shell shell;
    return shell.run();
}

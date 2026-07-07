#pragma once

#include "executor.h"
#include <string>

class Shell {
public:
    Shell();
    ~Shell();

    // Main REPL loop. Returns exit code.
    int run();

private:
    std::string build_prompt();
    void init_readline();

    Executor executor_;
};

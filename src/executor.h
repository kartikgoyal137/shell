#pragma once

#include "parser.h"
#include <set>
#include <string>
#include <vector>

class Executor {
public:
    Executor();

    // Execute a full command list. Returns exit status of last command.
    int execute(const CommandList& cmdlist);

    bool should_exit() const { return should_exit_; }
    int exit_code() const { return last_status_; }

private:
    int execute_pipeline(const Pipeline& pipeline);
    void setup_redirections(const std::vector<Redirection>& redirs);
    std::string find_in_path(const std::string& name);

    std::set<std::string> builtin_set_;
    int last_status_ = 0;
    bool should_exit_ = false;
};

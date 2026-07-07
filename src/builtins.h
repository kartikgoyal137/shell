#pragma once

#include <string>
#include <vector>
#include <set>

bool is_builtin(const std::string& cmd);

// Execute a builtin. Returns exit status.
int exec_builtin(const std::vector<std::string>& args,
                 const std::set<std::string>& builtin_set,
                 bool& should_exit);

const std::set<std::string>& builtin_names();

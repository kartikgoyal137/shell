#pragma once

#include <string>
#include <vector>

// Expands a single word: tilde expansion, variable expansion ($VAR, ${VAR}, $?, $$),
// and glob expansion (*, ?).
// Returns one or more words (glob can expand to multiple).
std::vector<std::string> expand_word(const std::string& word, int last_exit_status);

// Expands all args in a command's argument list.
std::vector<std::string> expand_args(const std::vector<std::string>& args, int last_exit_status);

// Expand only variables and tilde (no glob) - used for redirection targets.
std::string expand_single(const std::string& word, int last_exit_status);

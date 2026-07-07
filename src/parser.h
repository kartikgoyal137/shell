#pragma once

#include "token.h"
#include <string>
#include <vector>
#include <optional>

struct Redirection {
    int fd;             // 0 = stdin, 1 = stdout, 2 = stderr
    std::string target;
    bool append;
};

struct SimpleCommand {
    std::vector<std::string> args;
    std::vector<Redirection> redirections;
};

struct Pipeline {
    std::vector<SimpleCommand> commands;
};

enum class Connector { None, And, Or, Semi };

struct CommandList {
    struct Entry {
        Pipeline pipeline;
        Connector connector = Connector::None;
    };
    std::vector<Entry> entries;
};

// Parses a token stream into a structured command list.
std::optional<CommandList> parse(const std::vector<Token>& tokens);

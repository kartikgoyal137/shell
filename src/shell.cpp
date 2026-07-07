#include "shell.h"
#include "token.h"
#include "parser.h"
#include "builtins.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <filesystem>
#include <algorithm>
#include <readline/readline.h>
#include <readline/history.h>

namespace fs = std::filesystem;

static std::vector<std::string> g_completions;

static char* completion_generator(const char* text, int state) {
    static size_t idx, len;
    if (!state) { idx = 0; len = strlen(text); }

    while (idx < g_completions.size()) {
        const std::string& name = g_completions[idx++];
        if (name.compare(0, len, text) == 0)
            return strdup(name.c_str());
    }
    return nullptr;
}

static char** completion_function(const char* text, int start, int /*end*/) {
    rl_attempted_completion_over = 1;
    if (start > 0) {
        rl_attempted_completion_over = 0; // filename completion for arguments
        return nullptr;
    }
    return rl_completion_matches(text, completion_generator);
}

static void build_completion_list() {
    for (const auto& name : builtin_names())
        g_completions.push_back(name);

    const char* path_env = std::getenv("PATH");
    if (!path_env) return;

    std::string path_str(path_env);
    size_t start = 0;
    while (true) {
        size_t pos = path_str.find(':', start);
        std::string dir = path_str.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
        std::error_code ec;
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.is_regular_file(ec)) {
                    auto perms = entry.status(ec).permissions();
                    if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                        (perms & fs::perms::group_exec) != fs::perms::none ||
                        (perms & fs::perms::others_exec) != fs::perms::none)
                        g_completions.push_back(entry.path().filename().string());
                }
            }
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    std::sort(g_completions.begin(), g_completions.end());
    g_completions.erase(std::unique(g_completions.begin(), g_completions.end()),
                        g_completions.end());
}

Shell::Shell() {
    // Shell ignores SIGINT so Ctrl+C doesn't kill it (children get SIG_DFL)
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    init_readline();
}

Shell::~Shell() {
    const char* histfile = std::getenv("HISTFILE");
    if (histfile) write_history(histfile);
}

void Shell::init_readline() {
    const char* histfile = std::getenv("HISTFILE");
    if (histfile) read_history(histfile);
    build_completion_list();
    rl_attempted_completion_function = completion_function;
}

std::string Shell::build_prompt() {
    const char* user = std::getenv("USER");
    if (!user) {
        struct passwd* pw = getpwuid(getuid());
        user = pw ? pw->pw_name : "?";
    }

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = '\0';
    char* dot = strchr(hostname, '.');
    if (dot) *dot = '\0';

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "?");
    std::string dir(cwd);
    const char* home = std::getenv("HOME");
    if (home && dir.find(home) == 0)
        dir = "~" + dir.substr(strlen(home));

    // \001 / \002 are readline's markers for non-printing characters
    std::string prompt;
    prompt += "\001\033[1;32m\002";  // bold green
    prompt += user;
    prompt += "@";
    prompt += hostname;
    prompt += "\001\033[0m\002:";
    prompt += "\001\033[1;34m\002";  // bold blue
    prompt += dir;
    prompt += "\001\033[0m\002";
    prompt += (getuid() == 0 ? "# " : "$ ");
    return prompt;
}

int Shell::run() {
    char* line;
    while ((line = readline(build_prompt().c_str())) != nullptr) {
        if (strlen(line) == 0) { free(line); continue; }

        add_history(line);
        std::string input(line);
        free(line);

        auto tokens = tokenize(input);
        if (tokens.empty()) continue;

        auto cmdlist = parse(tokens);
        if (!cmdlist) continue;

        executor_.execute(*cmdlist);
        if (executor_.should_exit()) return executor_.exit_code();
    }

    std::cout << std::endl; // EOF
    return 0;
}

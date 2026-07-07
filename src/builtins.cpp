#include "builtins.h"
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <readline/history.h>
#include <pwd.h>

namespace fs = std::filesystem;

static const std::set<std::string> builtins = {
    "exit", "cd", "pwd", "echo", "type", "history", "export", "unset", "help"
};

const std::set<std::string>& builtin_names() { return builtins; }

bool is_builtin(const std::string& cmd) { return builtins.count(cmd) > 0; }

static int builtin_cd(const std::vector<std::string>& args) {
    std::string target;
    if (args.size() < 2 || args[1] == "~") {
        const char* home = std::getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (!home) {
            std::cerr << "ember: cd: HOME not set" << std::endl;
            return 1;
        }
        target = home;
    } else if (args[1] == "-") {
        const char* oldpwd = std::getenv("OLDPWD");
        if (!oldpwd) {
            std::cerr << "ember: cd: OLDPWD not set" << std::endl;
            return 1;
        }
        target = oldpwd;
        std::cout << target << std::endl;
    } else {
        target = args[1];
    }

    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)))
        setenv("OLDPWD", cwd, 1);

    if (chdir(target.c_str()) != 0) {
        std::cerr << "ember: cd: " << target << ": No such file or directory" << std::endl;
        return 1;
    }

    if (getcwd(cwd, sizeof(cwd)))
        setenv("PWD", cwd, 1);
    return 0;
}

static int builtin_echo(const std::vector<std::string>& args) {
    bool newline = true;
    size_t start = 1;
    if (args.size() > 1 && args[1] == "-n") { newline = false; start = 2; }

    for (size_t i = start; i < args.size(); ++i) {
        if (i > start) std::cout << ' ';
        std::cout << args[i];
    }
    if (newline) std::cout << '\n';
    std::cout << std::flush;
    return 0;
}

static int builtin_type(const std::vector<std::string>& args,
                        const std::set<std::string>& builtin_set) {
    if (args.size() < 2) {
        std::cerr << "ember: type: not enough arguments" << std::endl;
        return 1;
    }

    int ret = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& name = args[i];
        if (builtin_set.count(name)) {
            std::cout << name << " is a shell builtin" << std::endl;
            continue;
        }

        const char* path_env = std::getenv("PATH");
        if (!path_env) { ret = 1; continue; }

        std::string path_str(path_env);
        bool found = false;
        size_t start = 0, pos;
        while (true) {
            pos = path_str.find(':', start);
            std::string dir = path_str.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
            fs::path candidate = fs::path(dir) / name;
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec)) {
                auto perms = fs::status(candidate, ec).permissions();
                if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                    (perms & fs::perms::group_exec) != fs::perms::none ||
                    (perms & fs::perms::others_exec) != fs::perms::none) {
                    std::cout << name << " is " << candidate.string() << std::endl;
                    found = true;
                    break;
                }
            }
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        if (!found) { std::cerr << name << ": not found" << std::endl; ret = 1; }
    }
    return ret;
}

static int builtin_history(const std::vector<std::string>& args) {
    HIST_ENTRY** hist_list = history_list();
    if (!hist_list) return 0;

    if (args.size() > 1) {
        const std::string& flag = args[1];
        if (flag == "-c") { clear_history(); return 0; }
        if (flag == "-w") {
            const char* f = args.size() > 2 ? args[2].c_str() : std::getenv("HISTFILE");
            if (f) write_history(f);
            return 0;
        }
        if (flag == "-r") {
            const char* f = args.size() > 2 ? args[2].c_str() : std::getenv("HISTFILE");
            if (f) read_history(f);
            return 0;
        }
    }

    int total = 0;
    while (hist_list[total]) ++total;
    int n = total;
    if (args.size() > 1 && args[1].find_first_not_of("0123456789") == std::string::npos)
        n = std::stoi(args[1]);
    if (n > total) n = total;

    for (int i = total - n; hist_list[i]; ++i)
        std::cout << "  " << i + 1 << "  " << hist_list[i]->line << std::endl;
    return 0;
}

static int builtin_export(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        extern char** environ;
        for (char** env = environ; *env; ++env)
            std::cout << "declare -x " << *env << std::endl;
        return 0;
    }
    for (size_t i = 1; i < args.size(); ++i) {
        size_t eq = args[i].find('=');
        if (eq != std::string::npos)
            setenv(args[i].substr(0, eq).c_str(), args[i].substr(eq + 1).c_str(), 1);
    }
    return 0;
}

static int builtin_unset(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); ++i)
        unsetenv(args[i].c_str());
    return 0;
}

static int builtin_help() {
    std::cout << "Ember Shell - a Unix shell\n"
              << "Builtins: cd, pwd, echo, type, export, unset, history, help, exit\n"
              << "Operators: | (pipe), > >> < 2> 2>> (redirection), && || ; (logic)\n";
    return 0;
}

int exec_builtin(const std::vector<std::string>& args,
                 const std::set<std::string>& builtin_set,
                 bool& should_exit) {
    const std::string& cmd = args[0];

    if (cmd == "exit") {
        int code = 0;
        if (args.size() > 1) {
            try { code = std::stoi(args[1]); }
            catch (...) {
                std::cerr << "ember: exit: " << args[1] << ": numeric argument required" << std::endl;
                code = 2;
            }
        }
        const char* histfile = std::getenv("HISTFILE");
        if (histfile) write_history(histfile);
        should_exit = true;
        return code;
    }
    if (cmd == "cd")      return builtin_cd(args);
    if (cmd == "pwd")     { char b[4096]; if(getcwd(b,sizeof(b))) std::cout<<b<<std::endl; return 0; }
    if (cmd == "echo")    return builtin_echo(args);
    if (cmd == "type")    return builtin_type(args, builtin_set);
    if (cmd == "history") return builtin_history(args);
    if (cmd == "export")  return builtin_export(args);
    if (cmd == "unset")   return builtin_unset(args);
    if (cmd == "help")    return builtin_help();

    std::cerr << "ember: " << cmd << ": unknown builtin" << std::endl;
    return 1;
}

#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

Executor::Executor() {
    builtin_set_ = builtin_names();
}

std::string Executor::find_in_path(const std::string& name) {
    if (name.find('/') != std::string::npos) {
        std::error_code ec;
        if (fs::is_regular_file(name, ec)) return name;
        return "";
    }

    const char* path_env = std::getenv("PATH");
    if (!path_env) return "";

    std::string path_str(path_env);
    size_t start = 0;
    while (true) {
        size_t pos = path_str.find(':', start);
        std::string dir = path_str.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
        fs::path candidate = fs::path(dir) / name;
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            auto perms = fs::status(candidate, ec).permissions();
            if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                (perms & fs::perms::group_exec) != fs::perms::none ||
                (perms & fs::perms::others_exec) != fs::perms::none) {
                return candidate.string();
            }
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return "";
}

void Executor::setup_redirections(const std::vector<Redirection>& redirs) {
    for (const auto& redir : redirs) {
        std::string target = expand_single(redir.target, last_status_);
        int fd;
        if (redir.fd == 0) {
            fd = open(target.c_str(), O_RDONLY);
        } else {
            int flags = O_WRONLY | O_CREAT | (redir.append ? O_APPEND : O_TRUNC);
            fd = open(target.c_str(), flags, 0644);
        }
        if (fd < 0) {
            std::cerr << "ember: " << target << ": " << strerror(errno) << std::endl;
            _exit(1);
        }
        dup2(fd, redir.fd);
        close(fd);
    }
}

// Helper: save/restore file descriptors for in-process builtin redirections
struct SavedFDs {
    int fds[3] = {-1, -1, -1}; // stdin, stdout, stderr
    void save() { for (int i = 0; i < 3; ++i) fds[i] = dup(i); }
    void restore() {
        for (int i = 0; i < 3; ++i) {
            if (fds[i] >= 0) { dup2(fds[i], i); close(fds[i]); fds[i] = -1; }
        }
    }
};

static bool apply_redirections_inprocess(const std::vector<Redirection>& redirs, int last_status) {
    for (const auto& redir : redirs) {
        std::string target = expand_single(redir.target, last_status);
        int fd;
        if (redir.fd == 0) {
            fd = open(target.c_str(), O_RDONLY);
        } else {
            int flags = O_WRONLY | O_CREAT | (redir.append ? O_APPEND : O_TRUNC);
            fd = open(target.c_str(), flags, 0644);
        }
        if (fd < 0) {
            std::cerr << "ember: " << target << ": " << strerror(errno) << std::endl;
            return false;
        }
        dup2(fd, redir.fd);
        close(fd);
    }
    return true;
}

int Executor::execute_pipeline(const Pipeline& pipeline) {
    // Single command — run builtins in-process
    if (pipeline.commands.size() == 1) {
        SimpleCommand cmd = pipeline.commands[0];
        cmd.args = expand_args(cmd.args, last_status_);
        if (cmd.args.empty()) return 0;

        if (is_builtin(cmd.args[0])) {
            SavedFDs saved;
            if (!cmd.redirections.empty()) {
                saved.save();
                if (!apply_redirections_inprocess(cmd.redirections, last_status_)) {
                    saved.restore();
                    return 1;
                }
            }
            int status = exec_builtin(cmd.args, builtin_set_, should_exit_);
            saved.restore();
            return status;
        }

        // Single external command
        std::string path = find_in_path(cmd.args[0]);
        if (path.empty()) {
            std::cerr << "ember: " << cmd.args[0] << ": command not found" << std::endl;
            return 127;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("ember: fork"); return 1; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            setup_redirections(cmd.redirections);
            std::vector<char*> argv;
            for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            std::cerr << "ember: " << cmd.args[0] << ": " << strerror(errno) << std::endl;
            _exit(errno == ENOENT ? 127 : 126);
        }

        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }

    // Multi-command pipeline
    int n = static_cast<int>(pipeline.commands.size());
    std::vector<int> pipe_fds(2 * (n - 1));

    for (int i = 0; i < n - 1; ++i) {
        if (pipe(&pipe_fds[2 * i]) < 0) {
            perror("ember: pipe");
            return 1;
        }
    }

    std::vector<pid_t> pids;
    for (int i = 0; i < n; ++i) {
        SimpleCommand cmd = pipeline.commands[i];
        cmd.args = expand_args(cmd.args, last_status_);
        if (cmd.args.empty()) continue;

        const std::string& name = cmd.args[0];
        bool is_bi = is_builtin(name);
        if (!is_bi && find_in_path(name).empty()) {
            std::cerr << "ember: " << name << ": command not found" << std::endl;
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipe_fds[k]);
            for (pid_t p : pids) waitpid(p, nullptr, 0);
            return 127;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("ember: fork"); return 1; }

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            // Wire up pipes
            if (i > 0) dup2(pipe_fds[2 * (i - 1)], STDIN_FILENO);
            if (i < n - 1) dup2(pipe_fds[2 * i + 1], STDOUT_FILENO);
            for (int k = 0; k < 2 * (n - 1); ++k) close(pipe_fds[k]);

            setup_redirections(cmd.redirections);

            if (is_bi) {
                bool dummy = false;
                _exit(exec_builtin(cmd.args, builtin_set_, dummy));
            }

            std::vector<char*> argv;
            for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            std::cerr << "ember: " << name << ": " << strerror(errno) << std::endl;
            _exit(errno == ENOENT ? 127 : 126);
        }

        pids.push_back(pid);
    }

    // Close all pipe fds in parent
    for (int k = 0; k < 2 * (n - 1); ++k) close(pipe_fds[k]);

    // Wait for all children, return exit status of last
    int last_status = 0;
    for (pid_t pid : pids) {
        int status;
        waitpid(pid, &status, 0);
        last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }
    return last_status;
}

int Executor::execute(const CommandList& cmdlist) {
    for (size_t i = 0; i < cmdlist.entries.size(); ++i) {
        // Short-circuit: && skips on failure, || skips on success
        if (i > 0) {
            Connector prev = cmdlist.entries[i - 1].connector;
            if (prev == Connector::And && last_status_ != 0) continue;
            if (prev == Connector::Or  && last_status_ == 0) continue;
        }

        last_status_ = execute_pipeline(cmdlist.entries[i].pipeline);
        if (should_exit_) return last_status_;
    }
    return last_status_;
}

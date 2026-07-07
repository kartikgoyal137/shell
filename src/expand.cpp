#include "expand.h"
#include <cstdlib>
#include <unistd.h>
#include <glob.h>
#include <pwd.h>

static std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home) return home;
    struct passwd* pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "";
}

// Expand ~ at the beginning of a word
static std::string expand_tilde(const std::string& word) {
    if (word.empty() || word[0] != '~') return word;

    if (word.size() == 1 || word[1] == '/') {
        return get_home_dir() + word.substr(1);
    }

    // ~user expansion
    size_t slash = word.find('/');
    std::string username = word.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
    struct passwd* pw = getpwnam(username.c_str());
    if (pw) {
        std::string result = pw->pw_dir;
        if (slash != std::string::npos) result += word.substr(slash);
        return result;
    }
    return word;
}

// Expand $VAR, ${VAR}, $?, $$
static std::string expand_variables(const std::string& word, int last_exit_status) {
    std::string result;
    size_t i = 0;
    size_t len = word.size();

    while (i < len) {
        if (word[i] == '$' && i + 1 < len) {
            ++i;
            if (word[i] == '?') {
                result += std::to_string(last_exit_status);
                ++i;
            } else if (word[i] == '$') {
                result += std::to_string(getpid());
                ++i;
            } else if (word[i] == '{') {
                ++i;
                size_t start = i;
                while (i < len && word[i] != '}') ++i;
                std::string varname = word.substr(start, i - start);
                if (i < len) ++i; // skip }
                const char* val = std::getenv(varname.c_str());
                if (val) result += val;
            } else if (std::isalnum(word[i]) || word[i] == '_') {
                size_t start = i;
                while (i < len && (std::isalnum(word[i]) || word[i] == '_')) ++i;
                std::string varname = word.substr(start, i - start);
                const char* val = std::getenv(varname.c_str());
                if (val) result += val;
            } else {
                result += '$';
            }
        } else {
            result += word[i++];
        }
    }
    return result;
}

static bool has_glob_chars(const std::string& s) {
    for (char c : s) {
        if (c == '*' || c == '?' || c == '[') return true;
    }
    return false;
}

static std::vector<std::string> expand_glob(const std::string& pattern) {
    if (!has_glob_chars(pattern)) return {pattern};

    glob_t globbuf;
    int flags = GLOB_NOCHECK | GLOB_TILDE;
    int ret = glob(pattern.c_str(), flags, nullptr, &globbuf);

    std::vector<std::string> results;
    if (ret == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            results.emplace_back(globbuf.gl_pathv[i]);
        }
    } else {
        results.push_back(pattern);
    }
    globfree(&globbuf);
    return results;
}

std::vector<std::string> expand_word(const std::string& word, int last_exit_status) {
    std::string expanded = expand_tilde(word);
    expanded = expand_variables(expanded, last_exit_status);
    return expand_glob(expanded);
}

std::vector<std::string> expand_args(const std::vector<std::string>& args, int last_exit_status) {
    std::vector<std::string> result;
    for (const auto& arg : args) {
        auto expanded = expand_word(arg, last_exit_status);
        result.insert(result.end(), expanded.begin(), expanded.end());
    }
    return result;
}

std::string expand_single(const std::string& word, int last_exit_status) {
    std::string expanded = expand_tilde(word);
    return expand_variables(expanded, last_exit_status);
}

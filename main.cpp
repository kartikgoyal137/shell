#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <set>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <readline/readline.h>
#include <sys/wait.h> 
#include <readline/history.h>
#include <algorithm>

namespace fs = std::filesystem;

bool is_executable(const fs::path& p) {
    if (!fs::is_regular_file(p)) return false;
    auto perms = fs::status(p).permissions();
    return (perms & fs::perms::owner_exec) != fs::perms::none ||
           (perms & fs::perms::group_exec) != fs::perms::none ||
           (perms & fs::perms::others_exec) != fs::perms::none;
}

std::vector<std::string> parse_line(const std::string& input) {
    std::vector<std::string> result;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (size_t i=0; i<input.size(); ++i) {
        char c = input[i];
        if(c=='\\' && i+1<input.size()) {
          if(!in_single_quote && !in_double_quote) {
            current += input[i+1];
            i++;
            continue;
          }
          if(in_double_quote) {
            if(input[i+1]=='\"' || input[i+1]=='\\' || input[i+1]=='\$' || input[i+1]=='\`') {
              current+=input[i+1];
              i++;
              continue;
            }
          }
        }
        if (c == '\'') {
          if(in_double_quote) {current+=c; continue;}
            in_single_quote = !in_single_quote;
            continue;
        }
        else if(c == '\"') {
          if(in_single_quote) {current+=c; continue;}
          in_double_quote = !in_double_quote;
          
          continue;
        }
        if (c == ' ' && !in_single_quote && !in_double_quote) {
            if (!current.empty()) { //
                result.push_back(current);
                current.clear();
            }
            continue; 
        }
        current += c;
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

fs::path check_in_path(const std::vector<std::string>& paths, std::string arg1, bool& found) {
        try {
        for(const auto& path : paths) {
        for(const auto& cmd : fs::directory_iterator(path)) {
          if(cmd.path().filename() == arg1 && is_executable(cmd.path())) {
            found = true;
          }
          
          if(found) return cmd.path();
        }
        }
        } catch (const fs::filesystem_error& e) {
          std::cerr << e.what() << std::endl;
        }
        return {};
}

std::string shell_quote(const std::string& s) {
    std::string quoted_s = "'";
    for (char c : s) {
        if (c == '\'') {
            quoted_s += "'\\''";
        } else {
            quoted_s += c;
        }
    }
    quoted_s += "'";
    return quoted_s;
}

std::vector<char*> list_of_cmdGLOBAL = {
    "cat",
    "echo",
    "type",
    "pwd",
    "exit",
    "history"
};

char* cmd_name_gen(const char* text, int state) {
  static int list_index, len;
  char* cmd_name;

  if(!state) {
    list_index = 0;
    len = strlen(text); 
  }

  while((cmd_name=list_of_cmdGLOBAL[list_index++])) {
    if(strncmp(cmd_name, text, len) == 0) {
      return strdup(cmd_name);
    }
  }
  return NULL;
}

char** cmd_name_complete(const char* text, int start, int end) {
  rl_attempted_completion_over = 1;
  return rl_completion_matches(text, cmd_name_gen);
}

int fill_cmd_names() {
  const char* path_env = std::getenv("PATH");
  if (!path_env) {
      std::cerr << "PATH environment variable not found\n";
      return 1;
  }
  std::string path_str(path_env);
  size_t start=0, end=0;
  while((end=path_str.find(':', start))!=std::string::npos) {
    std::string dir_path = path_str.substr(start, end-start);
    if (fs::exists(dir_path) && fs::is_directory(dir_path)) {
            for (auto& entry : fs::directory_iterator(dir_path)) {
                if (is_executable(entry.path())) {
                    std::string filename = entry.path().filename().string();
                    char* cstr = new char[filename.size()+1];
                    std::strcpy(cstr, filename.c_str());
                    list_of_cmdGLOBAL.push_back(cstr);
                }
            }
    }
    start = end+1;
  }
  std::string dir_last = path_str.substr(start);
  if (fs::exists(dir_last) && fs::is_directory(dir_last)) {
            for (auto& entry : fs::directory_iterator(dir_last)) {
                if (is_executable(entry.path())) {
                    std::string filename = entry.path().filename().string();
                    char* cstr = new char[filename.size()+1];
                    std::strcpy(cstr, filename.c_str());
                    list_of_cmdGLOBAL.push_back(cstr);
                }
            }
    }

    list_of_cmdGLOBAL.push_back(NULL);

    return 0;
}

int fill_history() {
  const char* hist_env = std::getenv("HISTFILE");
  if (!hist_env) {
      return 1;
  }
  else {
  read_history(hist_env);
  }
  
  return 0;
}

void handle_echo(const std::vector<std::string>& parsed_line) {
  for(size_t i = 1; i < parsed_line.size(); ++i) {
        std::cout << parsed_line[i] << (i == parsed_line.size() - 1 ? "" : " ");
  }
  std::cout << std::endl;
}

void handle_pwd() {
      char buffer[1024]; getcwd(buffer, 1024);
      std::string cwd(buffer);
      std::cout << cwd << std::endl;
}

void handle_type(const std::vector<std::string>& args, const std::set<std::string>& list_of_cmd, const std::vector<std::string>& paths) {
  if (args.size() < 2) return;
    std::string arg1 = args[1];
  if(list_of_cmd.count(arg1)) {
        std::cout << arg1 << " is a shell builtin" << std::endl;
      }
      else {
        bool found = false;
        auto path = check_in_path(paths, arg1, found);
        if(found){
          std::cout << arg1 << " is " << path.string() << std::endl;
        }
        else {
          std::cout << arg1 << ": not found" << std::endl;
        }
}
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  fill_cmd_names();
  fill_history();

  static int last_history_append_len = history_length; 

  rl_attempted_completion_function = cmd_name_complete;

  std::set<std::string> list_of_cmd;
  list_of_cmd.insert("type");
  list_of_cmd.insert("exit");
  list_of_cmd.insert("echo");
  list_of_cmd.insert("pwd");
  list_of_cmd.insert("cd");
  list_of_cmd.insert("history");
  

  std::string path_env_str = getenv("PATH");
  std::vector<std::string>paths;
  size_t pos = 0;
  size_t start = 0;
  while(true) {
    pos = path_env_str.find(":", start);
    if(pos==std::string::npos) {
        paths.push_back(path_env_str.substr(start));
        break;
    }
    paths.push_back(path_env_str.substr(start, pos-start));
    start = pos+1;
  }

  char* inp;
  while((inp = readline("$ ")) != nullptr) {

    if (strlen(inp) > 0) {
      add_history(inp);
    }

    std::string input(inp);
    free(inp);
    
    std::vector<std::string> parsed_line = parse_line(input);
    if(parsed_line.empty()) continue;

    bool has_pipe = false;
    for (const auto& token : parsed_line) {
        if (token == "|") {
            has_pipe = true;
            break;
        }
    }
    
    if (has_pipe) {
      std::vector<std::vector<std::string>> segments;
      segments.emplace_back();
      for(const auto& token : parsed_line) {
          if (token == "|") {
              segments.emplace_back();
          } else {
              segments.back().push_back(token);
          }
      }
      
      int n = segments.size();
      std::vector<pid_t> pid_list;
      int pipes[n-1][2];

      for (int i = 0; i < n-1; i++) {
          if (pipe(pipes[i]) == -1) {
              perror("pipe");
              exit(1);
          }
      }

      for(int j=0; j<n; j++) {
        std::string cmd_name = segments[j][0];
        bool is_builtin = list_of_cmd.count(cmd_name);
        bool found_external = false;
        check_in_path(paths, cmd_name, found_external);

        if (is_builtin || found_external) {
          pid_t pid = fork();
          if(pid<0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
          }
          else if(pid==0) { 
            if (j > 0) {
                  dup2(pipes[j - 1][0], STDIN_FILENO);
            }
            if (j < n - 1) {
                dup2(pipes[j][1], STDOUT_FILENO);
            }
            for (int k = 0; k < n-1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            if (cmd_name == "echo") {
                handle_echo(segments[j]);
                exit(0); 
            } else if (cmd_name == "pwd") {
                handle_pwd();
                exit(0); 
            } else if (cmd_name == "type") {
                handle_type(segments[j], list_of_cmd, paths);
                exit(0);
            } else { 
                std::vector<char*> args;
                for (const auto& s : segments[j]) {
                    args.push_back(const_cast<char*>(s.c_str()));
                }
                args.push_back(nullptr);
                execvp(args[0], args.data());
                perror("execvp failed");
                exit(1);
            }
          } else {
             pid_list.push_back(pid);
          }
        } else {
          std::cout << cmd_name << ": command not found" << std::endl;
          break;
        }
      }
      for (int j = 0; j < n-1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
      }
      for(auto& k : pid_list) {
        int status;
        waitpid(k, &status, 0);
      }
    } else {
        std::string cmd_name = parsed_line[0];
        std::string arg1 = parsed_line.size() > 1 ? parsed_line[1] : "";

        if(cmd_name=="exit") {
            const char* path_hist = std::getenv("HISTFILE");
            if(arg1=="0") {
                if(path_hist) write_history(path_hist);
                return 0;
            }
        } else if(cmd_name=="pwd") {
            handle_pwd();
        } else if(cmd_name=="cd") {
            std::string target_dir = arg1;
            if(target_dir.empty() || target_dir == "~") {
                target_dir = getenv("HOME");
            }
            if(fs::exists(target_dir) && fs::is_directory(target_dir)) {
                fs::current_path(target_dir);
            } else {
                std::cout << "cd: " << arg1 << ": No such file or directory" << std::endl;
            }
        } else if(cmd_name=="echo") {
            handle_echo(parsed_line);
        } else if(cmd_name=="cat") {
            for (size_t i = 1; i < parsed_line.size(); ++i) {
                std::ifstream file(parsed_line[i]);
                if (file) {
                    std::cout << file.rdbuf();
                } else {
                    std::cerr << "cat: " << parsed_line[i] << ": No such file or directory\n";
                }
            }
        } else if(cmd_name=="type") {
            handle_type(parsed_line, list_of_cmd, paths);
        } else if(cmd_name=="history") {
            HIST_ENTRY **hist_list = history_list();
            if (hist_list) {
                if (arg1 == "-w") {
                    if (parsed_line.size() > 2) write_history(parsed_line[2].c_str());
                } else if (arg1 == "-a") {
                    if (parsed_line.size() > 2) {
                        int new_entries = history_length - last_history_append_len;
                        if(new_entries>0) append_history(new_entries, parsed_line[2].c_str());
                        last_history_append_len = history_length;
                    }
                } else if (arg1 == "-r") {
                    if (parsed_line.size() > 2) read_history(parsed_line[2].c_str());
                } else {
                     int l = 0;
                     while(hist_list[l]) l++;
                     int n = l;
                     if(!arg1.empty() && arg1.find_first_not_of("0123456789") == std::string::npos) {
                         n = std::stoi(arg1);
                     }
                     if (n > l) n = l;
                     for (int i = l - n; hist_list[i]; i++) {
                         std::cout << i + 1 << " " << hist_list[i]->line << std::endl;
                     }
                }
            }
        } else { 
            bool found = false;
            auto path = check_in_path(paths, cmd_name, found);
            if (found) {
                std::vector<char*> args;
                for(const auto& arg : parsed_line) {
                    args.push_back(const_cast<char*>(arg.c_str()));
                }
                args.push_back(nullptr);

                pid_t pid = fork();
                if(pid<0) {
                    perror("fork failed");
                    exit(EXIT_FAILURE);
                } else if(pid==0) {
                    execvp(args[0], args.data());
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                } else {
                    int status;
                    waitpid(pid, &status, 0);
                }
            } else {
                std::cout << cmd_name << ": command not found" << std::endl;
            }
        }
    }
  }
  return 0;
}
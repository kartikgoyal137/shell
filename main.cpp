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

fs::path check_in_path(std::vector<std::string> paths, std::string arg1, bool& found) {
        try {
        for(auto& path : paths) {
        for(auto& cmd : fs::directory_iterator(path)) {
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
  

  std::string input;
  std::string path = std::getenv("PATH");
  std::vector<std::string>paths;
  size_t pos = 0;
  size_t start = 0;
  while(true) {
    pos = path.find(":", start);
    if(pos==std::string::npos) break;
    paths.push_back(path.substr(start, pos-start));
    start = pos+1;
  }

  char* inp;
  while((inp = readline("$ ")) != nullptr) {

    if (strlen(inp) > 0) {
      add_history(inp);
    }

    std::string input(inp);
    free(inp);
    
    std::vector<std::string> parsed_line;
    parsed_line = parse_line(input);
    if(parsed_line.empty()) continue;
    std::string cmd_name = parsed_line[0];
    std::string arg1 = "";
    if(parsed_line.size()>1) arg1 = parsed_line[1];
    
    if(cmd_name=="exit") {
      HIST_ENTRY **hist_list = history_list();
      
      int l = 0;
      while(hist_list[l]) {
        l++;
      }
      const char* path_hist = std::getenv("HISTFILE");
      if(arg1=="0") {
        if(path_hist)
        write_history(path_hist);
        return 0;
      }
    }
    else if(cmd_name=="pwd") {
      char buffer[1024]; getcwd(buffer, 1024);
      std::string cwd(buffer);
      std::cout << cwd << std::endl;
    }
    else if(cmd_name=="cd") {
      
      if(arg1=="~") {
          arg1 = std::getenv("HOME");
          fs::current_path(arg1);
        }
      else if(fs::exists(arg1) && fs::is_directory(arg1)) {
        fs::current_path(arg1);
      }
      else {
        std::cout << "cd: " << arg1 << ": No such file or directory" << std::endl;
      }
      
    }
    else if(cmd_name=="echo") {
      for(auto& i : parsed_line) {
        if(i == "echo") continue;
        std::cout << i << " ";
      }
       
      std::cout << std::endl;
    }
    else if(cmd_name=="cat") {
      for (const auto &filename : parsed_line) {
        if(filename=="cat") continue;
        std::ifstream file(filename);
        if (file) {
            std::cout << file.rdbuf();
        } else {
            std::cerr << "cat: " << filename << ": No such file or directory\n";
        }
      }
        std::cout << std::flush;
        }
    else if(cmd_name=="type") {

      if(list_of_cmd.find(arg1)!=list_of_cmd.end()) {
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
    else if(cmd_name=="history") {

      HIST_ENTRY **hist_list = history_list();
      
      int l = 0;
      while(hist_list[l]) {
        l++;
      }
      int n = l;
      if(arg1!="" && arg1!="-r" && arg1!="-w" && arg1!="-a") n = std::stoi(arg1);
      

      if (hist_list) {

          if (arg1 == "-w") {
            write_history(parsed_line[2].c_str());
            continue;
        } else if (arg1 == "-a") {
          int new_entries = history_length - last_history_append_len;
            if(new_entries>0) append_history(new_entries, parsed_line[2].c_str());
            last_history_append_len = history_length;
            continue;
        } else if (arg1 == "-r") {
            read_history(parsed_line[2].c_str());
            continue;
        }

        for (int i = l-n; i<l; i++) {
          std::cout << i+1 << " " << hist_list[i]->line << std::endl;
        }
      }
    }
    else {
      bool found = false;
      auto path = check_in_path(paths, cmd_name, found);
      if (found) {
        std::vector<char*> args;
        args.push_back(const_cast<char*>(parsed_line[0].c_str()));
        for (size_t i = 1; i < parsed_line.size(); ++i) {
            args.push_back(const_cast<char*>(parsed_line[i].c_str()));
        }
        args.push_back(nullptr);
        pid_t pid = fork();
        if(pid<0) {
          perror("fork failed");
          exit(EXIT_FAILURE);
        }
        else if(pid==0) {
          execvp(args[0], args.data());
          perror("execvp failed");
          exit(EXIT_FAILURE);
        }
        else {
          int status;
          waitpid(pid, &status, 0);
        }
      } else {
        std::cout << cmd_name << ": command not found" << std::endl;
      }
    }
    
    
  }
  
}

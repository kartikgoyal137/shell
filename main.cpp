#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <set>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <unistd.h>

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

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::set<std::string> list_of_cmd;
  list_of_cmd.insert("type");
  list_of_cmd.insert("exit");
  list_of_cmd.insert("echo");
  list_of_cmd.insert("pwd");
  list_of_cmd.insert("cd");
  

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

  while(true) {
    std::cout << "$ ";
    std::getline(std::cin, input);
    std::vector<std::string> parsed_line;
    parsed_line = parse_line(input);
    std::string cmd_name = parsed_line[0];
    std::string arg1 = "";
    if(parsed_line.size()>1) arg1 = parsed_line[1];
    
    if(cmd_name=="exit") {
      if(arg1=="0") return 0;
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
    else {
      bool found = false;
      auto path = check_in_path(paths, cmd_name, found).filename();
      if (found) {
        std::string command_to_run = shell_quote(path.string());
        for (size_t i = 1; i < parsed_line.size(); ++i) {
            command_to_run += " " + shell_quote(parsed_line[i]);
        }
        std::system(command_to_run.c_str());
      } else {
        std::cout << cmd_name << ": command not found" << std::endl;
      }
    }
    
    
  }
  
}

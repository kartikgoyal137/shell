#include <iostream>
#include <string>
#include <sstream>
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

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::set<std::string> list_of_cmd;
  list_of_cmd.insert("type");
  list_of_cmd.insert("exit");
  list_of_cmd.insert("echo");
  list_of_cmd.insert("pwd");
  list_of_cmd.insert("cd");

  // Uncomment this block to pass the first stage
  

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
    std::istringstream cl(input);
    size_t pos = input.find(" ");
    std::string cmd_name = input.substr(0,pos);
    if(cmd_name=="exit") {
      if(input.substr(pos+1, 1)=="0") break;
    }
    else if(cmd_name=="pwd") {
      char buffer[1024]; getcwd(buffer, 1024);
      std::string cwd(buffer);
      std::cout << cwd << std::endl;
    }
    else if(cmd_name=="cd") {
      std::string arg1 = input.substr(pos+1);
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
      std::string arg1 = input.substr(pos+1);
      std::cout << arg1 << std::endl;
    }
    else if(cmd_name=="type") {
      std::string arg1 = input.substr(pos+1);

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
      auto path = check_in_path(paths, cmd_name, found);
      if(found) {
        std::string arg1 = input.substr(pos+1);
        std::string exe_0 = path.filename().string() + " " + arg1;
        int exe = std::system(exe_0.c_str());
      }
      else {
        std::cout << input << ": command not found" << std::endl;
      }
      
    }

    
  }
  
}

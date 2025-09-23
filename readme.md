# Ember

CppShell is a lightweight, custom command-line shell written in C++. It mimics the core functionality of traditional UNIX shells like bash, providing an interactive interface to the operating system. This project demonstrates fundamental concepts of process management, inter-process communication, and system calls.

## Features

* **Interactive Command Line:** A stable and interactive prompt using the GNU `readline` library.
* **Command History:** Persistent command history that is saved to a file (`$HISTFILE`) on exit and loaded on start.
* **Tab Completion:** Basic auto-completion for built-in and external commands found in the user's `$PATH`.
* **Command Execution:**
    * Handles execution of external commands by searching the `$PATH` environment variable.
    * Robust parsing of commands with arguments, including support for single (`'`) and double (`"`) quotes.
* **Pipelines (`|`):** Full support for multi-command pipelines, allowing the standard output of one command to be piped to the standard input of another. This works for both external and built-in commands.
* **Built-in Commands:** A suite of standard built-in commands are implemented directly within the shell for efficiency:
    * `exit <code>`: Exits the shell.
    * `cd <directory>`: Changes the current working directory. Supports `~` for the home directory.
    * `pwd`: Prints the current working directory.
    * `echo <args...>`: Prints arguments to standard output.
    * `type <command>`: Indicates how a command would be interpreted (as a shell built-in or an external executable).
    * `history [n]`: Displays the command history. Supports flags like `-w`, `-r`, and `-a` for file operations.

## Prerequisites

To build and run CppShell, you will need:

* A C++ compiler that supports C++17 (for `std::filesystem`), such as `g++`.
* The GNU `readline` library (`libreadline-dev` or `readline-devel` package).

## Building and Running

1.  **Clone the repository:**
    ```bash
    git clone https://your-repo-url/CppShell.git
    cd CppShell
    ```

2.  **Compile the source code:**
    Use the following command to compile `main.cpp`:
    ```bash
    g++ main.cpp -o shell -lreadline -lhistory -std=c++17
    ```

3.  **Run the shell:**
    Execute the compiled binary to start your shell session.
    ```bash
    ./shell
    ```
    You will be greeted with the `$` prompt.

## Future Improvements

* **I/O Redirection:** Implement input (`<`) and output (`>`, `>>`) redirection.
* **Job Control:** Add support for running processes in the background (`&`) and managing them with commands like `jobs`, `fg`, and `bg`.
* **Variable Expansion:** Implement support for shell variables and environment variable expansion (e.g., `echo $USER`).
* **Code Refactoring:** Break down the monolithic `main.cpp` file into a more modular structure with classes for parsing, command execution, and job management.
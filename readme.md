# Ember Shell

A Unix shell written in C++17 that implements core shell semantics: tokenization, AST-based parsing, process execution with pipeline plumbing, I/O redirection, and word expansion.

## Architecture

```
Input -> Tokenizer -> Parser -> Expander -> Executor
            |            |          |           |
         token.h     parser.h   expand.h   executor.h
```

- **Tokenizer** (`token.h/cpp`) — Splits raw input into tokens, handling single/double quoting, backslash escapes, and multi-character operators (`&&`, `||`, `>>`, `2>`).
- **Parser** (`parser.h/cpp`) — Builds a structured AST: `CommandList` -> `Pipeline` -> `SimpleCommand`, with explicit redirection nodes attached to each command.
- **Expander** (`expand.h/cpp`) — Tilde expansion (`~`, `~user`), variable expansion (`$VAR`, `${VAR}`, `$?`, `$$`), and glob expansion (`*`, `?`) via POSIX `glob(3)`.
- **Executor** (`executor.h/cpp`) — Walks the AST. Forks processes, sets up pipes with `dup2`, applies I/O redirections, and handles `&&`/`||` short-circuit logic.
- **Builtins** (`builtins.h/cpp`) — Commands that run in-process: `cd`, `exit`, `export`, `unset`, `echo`, `pwd`, `type`, `history`, `help`.
- **Shell** (`shell.h/cpp`) — REPL loop, signal setup, readline/history integration, and prompt rendering.

## Features

- **Pipelines**: `cmd1 | cmd2 | cmd3` with correct pipe plumbing and fd cleanup.
- **I/O Redirection**: `< file`, `> file`, `>> file`, `2> file`, `2>> file`. Works with both builtins and external commands.
- **Logical operators**: `&&` (run next on success), `||` (run next on failure), `;` (always run next).
- **Variable expansion**: `$VAR`, `${VAR}`, `$?` (last exit status), `$$` (shell PID).
- **Tilde expansion**: `~` -> `$HOME`, `~user` -> that user's home.
- **Glob expansion**: `*`, `?`, `[...]` patterns.
- **Quoting**: Single quotes (literal), double quotes (with `\` escapes for `"`, `\`, `$`, `` ` ``).
- **Signal handling**: Shell ignores SIGINT/SIGQUIT; child processes get default handlers.
- **Tab completion**: Command names for the first word, filenames for arguments.
- **Persistent history**: Loaded from / saved to `$HISTFILE`.

## Building

Requires: C++17 compiler, `libreadline-dev`.

```bash
make            # optimized build
make debug      # debug build with ASan + UBSan
./ember
```

## Design Decisions

- **AST-based execution** rather than direct string manipulation. The tokenizer/parser/executor split makes it straightforward to add new syntax without touching execution logic.
- **Expansion happens after parsing**, so glob results can't be re-interpreted as operators.
- **Builtins run in-process** for single commands (so `cd`, `export` etc. can modify shell state), but fork like external commands when inside a pipeline.
- **Redirections for builtins** use fd save/restore (`dup`/`dup2`) to avoid forking unnecessarily.

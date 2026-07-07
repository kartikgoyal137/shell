#pragma once

#include <string>
#include <vector>

enum class TokenType {
    Word,
    Pipe,          // |
    RedirIn,       // <
    RedirOut,      // >
    RedirAppend,   // >>
    RedirErr,      // 2>
    RedirErrAppend,// 2>>
    And,           // &&
    Or,            // ||
    Semi,          // ;
};

struct Token {
    TokenType type;
    std::string value;
};

// Tokenizes raw input into a stream of tokens.
// Handles single/double quoting, backslash escapes, and operator recognition.
std::vector<Token> tokenize(const std::string& input);

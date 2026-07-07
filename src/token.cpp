#include "token.h"

static bool is_operator_start(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>';
}

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t len = input.size();

    while (i < len) {
        if (input[i] == ' ' || input[i] == '\t') { ++i; continue; }
        if (input[i] == '#') break;

        // Operators
        if (is_operator_start(input[i])) {
            // 2> and 2>> : if we just pushed "2" as a word, reinterpret it
            if (input[i] == '>' && !tokens.empty() &&
                tokens.back().type == TokenType::Word && tokens.back().value == "2") {
                tokens.pop_back();
                if (i + 1 < len && input[i + 1] == '>') {
                    tokens.push_back({TokenType::RedirErrAppend, "2>>"});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::RedirErr, "2>"});
                    ++i;
                }
                continue;
            }

            if (input[i] == '|') {
                if (i + 1 < len && input[i + 1] == '|') {
                    tokens.push_back({TokenType::Or, "||"}); i += 2;
                } else {
                    tokens.push_back({TokenType::Pipe, "|"}); ++i;
                }
            } else if (input[i] == '&') {
                if (i + 1 < len && input[i + 1] == '&') {
                    tokens.push_back({TokenType::And, "&&"}); i += 2;
                } else {
                    ++i; // lone & ignored (no background support)
                }
            } else if (input[i] == ';') {
                tokens.push_back({TokenType::Semi, ";"}); ++i;
            } else if (input[i] == '<') {
                tokens.push_back({TokenType::RedirIn, "<"}); ++i;
            } else if (input[i] == '>') {
                if (i + 1 < len && input[i + 1] == '>') {
                    tokens.push_back({TokenType::RedirAppend, ">>"}); i += 2;
                } else {
                    tokens.push_back({TokenType::RedirOut, ">"}); ++i;
                }
            }
            continue;
        }

        // Word (possibly quoted)
        std::string word;
        while (i < len && input[i] != ' ' && input[i] != '\t' &&
               !is_operator_start(input[i]) && input[i] != '#') {

            if (input[i] == '\\' && i + 1 < len) {
                word += input[i + 1]; i += 2;
            } else if (input[i] == '\'') {
                ++i;
                while (i < len && input[i] != '\'') word += input[i++];
                if (i < len) ++i;
            } else if (input[i] == '"') {
                ++i;
                while (i < len && input[i] != '"') {
                    if (input[i] == '\\' && i + 1 < len) {
                        char next = input[i + 1];
                        if (next == '"' || next == '\\' || next == '$' || next == '`') {
                            word += next; i += 2; continue;
                        }
                    }
                    word += input[i++];
                }
                if (i < len) ++i;
            } else {
                word += input[i++];
            }
        }

        if (!word.empty())
            tokens.push_back({TokenType::Word, std::move(word)});
    }

    return tokens;
}

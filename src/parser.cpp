#include "parser.h"
#include <iostream>

static bool is_redirect_token(TokenType t) {
    return t == TokenType::RedirIn || t == TokenType::RedirOut ||
           t == TokenType::RedirAppend || t == TokenType::RedirErr ||
           t == TokenType::RedirErrAppend;
}

static bool is_connector(TokenType t) {
    return t == TokenType::And || t == TokenType::Or || t == TokenType::Semi;
}

std::optional<CommandList> parse(const std::vector<Token>& tokens) {
    CommandList result;
    size_t i = 0;
    size_t len = tokens.size();
    if (len == 0) return result;

    while (i < len) {
        Pipeline pipeline;

        while (true) {
            SimpleCommand cmd;

            while (i < len && tokens[i].type != TokenType::Pipe &&
                   !is_connector(tokens[i].type)) {

                if (is_redirect_token(tokens[i].type)) {
                    Redirection redir;
                    TokenType rt = tokens[i].type;
                    redir.fd = (rt == TokenType::RedirIn) ? 0 :
                               (rt == TokenType::RedirErr || rt == TokenType::RedirErrAppend) ? 2 : 1;
                    redir.append = (rt == TokenType::RedirAppend || rt == TokenType::RedirErrAppend);

                    ++i;
                    if (i >= len || tokens[i].type != TokenType::Word) {
                        std::cerr << "ember: syntax error near unexpected token `newline'" << std::endl;
                        return std::nullopt;
                    }
                    redir.target = tokens[i].value;
                    cmd.redirections.push_back(std::move(redir));
                    ++i;
                } else if (tokens[i].type == TokenType::Word) {
                    cmd.args.push_back(tokens[i].value);
                    ++i;
                } else {
                    break;
                }
            }

            if (cmd.args.empty() && cmd.redirections.empty()) {
                if (pipeline.commands.empty()) break;
                std::cerr << "ember: syntax error near unexpected token `|'" << std::endl;
                return std::nullopt;
            }

            pipeline.commands.push_back(std::move(cmd));

            if (i < len && tokens[i].type == TokenType::Pipe) { ++i; continue; }
            break;
        }

        if (pipeline.commands.empty()) break;

        Connector conn = Connector::None;
        if (i < len && is_connector(tokens[i].type)) {
            if (tokens[i].type == TokenType::And) conn = Connector::And;
            else if (tokens[i].type == TokenType::Or) conn = Connector::Or;
            else conn = Connector::Semi;
            ++i;
        }

        result.entries.push_back({std::move(pipeline), conn});
    }

    return result;
}

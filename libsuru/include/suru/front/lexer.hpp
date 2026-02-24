#pragma once

#include <string_view>
#include <vector>

#include "suru/front/token.hpp"

namespace suru::front {

class Lexer {
public:
    explicit Lexer(std::string_view source);
    std::vector<Token> tokenize();

private:
    bool at_end() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skip_ignored();
    Token scan_token();
    Token scan_identifier_or_keyword(SourceLocation loc);
    Token scan_number(SourceLocation loc);
    Token scan_string(SourceLocation loc, char quote);

    std::string_view source_;
    std::size_t index_ {0};
    std::size_t line_ {1};
    std::size_t column_ {1};
};

} // namespace suru::front

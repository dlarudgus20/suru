#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "suru/front/token.hpp"

namespace suru::front {

struct Diagnostic {
    SourceLocation location;
    std::string message;
};

struct ParseNode {
    std::string kind;
    SourceLocation location;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<std::pair<std::string, ParseNode>> nodes;
    std::vector<std::pair<std::string, std::vector<ParseNode>>> lists;
};

struct ParseTree {
    ParseNode root;
};

struct ParseResult {
    ParseTree tree;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return diagnostics.empty();
    }
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ~Parser();
    Parser(Parser&&) noexcept;
    Parser& operator=(Parser&&) noexcept;
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    ParseResult run();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace suru::front

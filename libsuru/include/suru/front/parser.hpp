#pragma once

#include <cstddef>
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

enum class ParseStatus {
    Ok,
    Incomplete,
    Error,
};

struct ParseResult {
    ParseStatus status {ParseStatus::Error};
    ParseTree tree;
    std::vector<Diagnostic> diagnostics;
    std::string filename;

    [[nodiscard]] bool ok() const {
        return status == ParseStatus::Ok;
    }
};

ParseResult parse_tokens(std::vector<Token> tokens);

} // namespace suru::front

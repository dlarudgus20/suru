#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "suru/front/ast.hpp"

namespace suru::front {

struct Diagnostic {
    SourceLocation location;
    std::string message;
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

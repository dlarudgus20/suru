#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "suru/front/parser.hpp"

namespace suru::front {

enum class ParseStatus {
    Ok,
    Incomplete,
    Error,
};

struct ParseSessionResult {
    ParseStatus status {ParseStatus::Error};
    ParseTree tree;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return status == ParseStatus::Ok;
    }
};

class ParserSession {
public:
    ParseSessionResult parse_fragment(std::string_view source_fragment);
    void reset();

private:
    std::string buffer_;
};

ParseResult parse(std::string_view source);

} // namespace suru::front

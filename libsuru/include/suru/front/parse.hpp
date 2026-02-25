#pragma once

#include <string>
#include <string_view>

#include "suru/front/parser.hpp"

namespace suru::front {

struct ParseContext {
    std::string filename;
    SourceLocation next_location {};
    SourceLocation buffer_start {};
    std::string pending_source;

    explicit ParseContext(std::string_view filename = "<anon>") : filename(filename) {}
};

ParseResult parse(std::string_view source, std::string_view filename = "<anon>");
ParseResult parse(std::string_view source_fragment, ParseContext& context);

} // namespace suru::front

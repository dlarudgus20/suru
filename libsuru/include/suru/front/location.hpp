#pragma once

#include <cstddef>

namespace suru::front {

struct SourceLocation {
    std::size_t line {1};
    std::size_t column {1};
};

// Half-open source interval [begin, end).
struct SourceRange {
    SourceLocation begin;
    SourceLocation end;
};

} // namespace suru::front

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/bytecode.hpp"

namespace suru::front {

struct CompileOptions {
    bool emit_print_for_print_stmt {true};
};

struct SourceLocation {
    std::size_t line {1};
    std::size_t column {1};
};

struct Diagnostic {
    SourceLocation location;
    std::string message;
};

struct CompileResult {
    suru::vm::BytecodeModule module;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return diagnostics.empty();
    }
};

CompileResult compile(std::string_view source, const CompileOptions& options = {});

} // namespace suru::front

#pragma once

#include <string>
#include <vector>

#include "suru/front/ast.hpp"
#include "suru/front/parser.hpp"
#include "suru/front/semantic.hpp"
#include "suru/ir/codeunit.hpp"

namespace suru::front {

struct CompileResult {
    suru::ir::CodeUnit code;
    SemanticModel semantics;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

[[nodiscard]] CompileResult compile(const Ast& ast);
[[nodiscard]] CompileResult compile(const Ast& ast, const SemanticModel& semantics);

} // namespace suru::front

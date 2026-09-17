#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "suru/front/ast.hpp"
#include "suru/front/parser.hpp"
#include "suru/ir/codeunit.hpp"

namespace suru::front {

enum class BindingKind : std::uint8_t {
    Local,
    Upvalue,
    Global,
};

struct ResolvedBinding {
    BindingKind kind {BindingKind::Global};
    std::uint8_t index {0};
    std::string global_name;
};

struct FunctionInfo {
    NodeId body {0};
    std::uint8_t arity {0};
    bool vararg {false};
    std::uint8_t local_slots {0};
    std::vector<suru::ir::UpvalueInfo> upvalues;
};

struct LoopTarget {
    NodeId loop_statement {0};
    std::optional<std::uint8_t> close_base;
};

struct SemanticModel {
    std::unordered_map<NodeId, ResolvedBinding> bindings;
    std::unordered_map<NodeId, FunctionInfo> functions;
    std::unordered_map<NodeId, std::vector<std::uint8_t>> declarations;
    std::unordered_map<NodeId, ResolvedBinding> function_roots;
    std::unordered_map<NodeId, std::uint8_t> block_bases;
    std::unordered_map<NodeId, LoopTarget> loop_targets;
};

struct ResolveResult {
    SemanticModel model;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

[[nodiscard]] ResolveResult resolve(const Ast& ast);

} // namespace suru::front

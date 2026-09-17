#include "suru/front/semantic.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace suru::front {
namespace {

struct CaptureSource {
    bool global {false};
    suru::ir::UpvalueSource source {suru::ir::UpvalueSource::Local};
    std::uint8_t index {0};
};

struct FunctionContext {
    FunctionContext* parent {nullptr};
    NodeId body {0};
    bool vararg {false};
    std::uint16_t next_slot {0};
    std::vector<std::unordered_map<std::string, std::uint8_t>> scopes;
    std::vector<suru::ir::UpvalueInfo> upvalues;

    std::optional<std::uint8_t> local(std::string_view name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (const auto found = it->find(std::string(name)); found != it->end()) return found->second;
        }
        return std::nullopt;
    }

    std::uint8_t ensure_upvalue(CaptureSource source) {
        for (std::size_t i = 0; i < upvalues.size(); ++i) {
            if (upvalues[i].source == source.source && upvalues[i].index == source.index) {
                return static_cast<std::uint8_t>(i);
            }
        }
        upvalues.push_back({source.source, source.index});
        return static_cast<std::uint8_t>(upvalues.size() - 1U);
    }

    CaptureSource capture(std::string_view name) {
        if (const auto slot = local(name)) return {false, suru::ir::UpvalueSource::Local, *slot};
        if (parent == nullptr) return {true, {}, 0};
        CaptureSource source = parent->capture(name);
        if (source.global) return source;
        return {false, suru::ir::UpvalueSource::Upvalue, ensure_upvalue(source)};
    }
};

struct LoopContext {
    NodeId statement {0};
    std::optional<std::string> label;
    std::uint8_t close_base {0};
    bool repeat {false};
    std::size_t body_scope_depth {0};
};

class Resolver {
public:
    ResolveResult run(const Ast& ast) {
        FunctionContext root;
        root.body = ast.root.id;
        root.scopes.emplace_back();
        function_ = &root;
        resolve_block(ast.root);
        model_.functions.emplace(ast.root.id, FunctionInfo {
            ast.root.id, 0, false, checked_slots(root.next_slot, ast.root.range.begin), root.upvalues
        });
        return {std::move(model_), std::move(diagnostics_)};
    }

private:
    std::uint8_t checked_slots(std::uint16_t value, SourceLocation location) {
        if (value > 255) diagnostic(location, "function requires more than 255 registers");
        return static_cast<std::uint8_t>(std::min<std::uint16_t>(value, 255));
    }

    std::uint8_t allocate(std::string name, SourceLocation location) {
        if (function_->next_slot >= 255) {
            diagnostic(location, "function requires more than 255 local registers");
            return 254;
        }
        const auto slot = static_cast<std::uint8_t>(function_->next_slot++);
        function_->scopes.back()[std::move(name)] = slot;
        return slot;
    }

    void diagnostic(SourceLocation location, std::string message) {
        diagnostics_.push_back({location, std::move(message)});
    }

    void resolve_block(const Block& block) {
        const auto base = checked_slots(function_->next_slot, block.range.begin);
        model_.block_bases[block.id] = base;
        block_bases_.push_back(base);
        function_->scopes.emplace_back();
        for (const auto& stmt : block.statements) resolve_stmt(*stmt);
        function_->scopes.pop_back();
        block_bases_.pop_back();
    }

    void resolve_loop_block(const Block& block, NodeId declaration, const std::vector<std::string>& names) {
        model_.block_bases[block.id] = checked_slots(function_->next_slot, block.range.begin);
        block_bases_.push_back(model_.block_bases[block.id]);
        function_->scopes.emplace_back();
        std::vector<std::uint8_t> slots;
        for (const auto& name : names) slots.push_back(allocate(name, block.range.begin));
        model_.declarations[declaration] = std::move(slots);
        for (const auto& stmt : block.statements) resolve_stmt(*stmt);
        function_->scopes.pop_back();
        block_bases_.pop_back();
    }

    void resolve_repeat_block(const RepeatStmt& node, NodeId statement_id) {
        const Block& block = *node.block;
        const std::uint8_t close_base = checked_slots(function_->next_slot, block.range.begin);
        model_.block_bases[block.id] = close_base;
        function_->scopes.emplace_back();
        block_bases_.push_back(close_base);
        loops_.push_back({statement_id, node.label, close_base, true, block_bases_.size()});
        for (const auto& stmt : block.statements) resolve_stmt(*stmt);
        resolve_expr(*node.condition);
        function_->scopes.pop_back();
        loops_.pop_back();
        block_bases_.pop_back();
    }

    void resolve_function(const FunctionBody& body, bool implicit_self) {
        FunctionContext child;
        child.parent = function_;
        child.body = body.id;
        child.vararg = body.vararg;
        child.scopes.emplace_back();
        FunctionContext* saved = function_;
        function_ = &child;

        std::uint16_t arity = body.parameters.size() + (implicit_self ? 1U : 0U);
        if (arity > 254) diagnostic(body.range.begin, "function arity exceeds 254");
        if (implicit_self) allocate("self", body.range.begin);
        for (const auto& parameter : body.parameters) allocate(parameter, body.range.begin);
        resolve_block(*body.block);

        if (child.upvalues.size() > 255) {
            diagnostic(body.range.begin, "function captures more than 255 upvalues");
        }

        model_.functions.emplace(body.id, FunctionInfo {
            body.id,
            static_cast<std::uint8_t>(std::min<std::uint16_t>(arity, 254)),
            body.vararg,
            checked_slots(child.next_slot, body.range.begin),
            child.upvalues,
        });
        function_ = saved;
    }

    ResolvedBinding resolve_name(std::string_view name) {
        if (const auto local = function_->local(name)) return {BindingKind::Local, *local, {}};
        if (function_->parent != nullptr) {
            CaptureSource source = function_->parent->capture(name);
            if (!source.global) return {BindingKind::Upvalue, function_->ensure_upvalue(source), {}};
        }
        return {BindingKind::Global, 0, std::string(name)};
    }

    void resolve_expr(const Expr& expr) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, VarargExpr>) {
                if (!function_->vararg) diagnostic(expr.range.begin, "'...' is only valid in a vararg function");
            } else if constexpr (std::is_same_v<T, NameExpr>) {
                model_.bindings[expr.id] = resolve_name(node.name);
            } else if constexpr (std::is_same_v<T, UnaryExpr>) resolve_expr(*node.operand);
            else if constexpr (std::is_same_v<T, BinaryExpr>) { resolve_expr(*node.left); resolve_expr(*node.right); }
            else if constexpr (std::is_same_v<T, GroupExpr>) resolve_expr(*node.expression);
            else if constexpr (std::is_same_v<T, IndexExpr>) { resolve_expr(*node.object); resolve_expr(*node.key); }
            else if constexpr (std::is_same_v<T, FieldExpr>) resolve_expr(*node.object);
            else if constexpr (std::is_same_v<T, CallExpr>) {
                resolve_expr(*node.callee); for (const auto& arg : node.arguments) resolve_expr(*arg);
            } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
                resolve_expr(*node.receiver); for (const auto& arg : node.arguments) resolve_expr(*arg);
            } else if constexpr (std::is_same_v<T, FunctionExpr>) resolve_function(*node.body, false);
            else if constexpr (std::is_same_v<T, ArrayExpr>) {
                for (const auto& item : node.elements) resolve_expr(*item);
            } else if constexpr (std::is_same_v<T, TableExpr>) {
                for (const auto& field : node.fields) { resolve_expr(*field.key); resolve_expr(*field.value); }
            }
        }, expr.kind);
    }

    void resolve_jump(const Stmt& stmt, const std::optional<std::string>& label, bool is_continue) {
        for (auto it = loops_.rbegin(); it != loops_.rend(); ++it) {
            if (!label || it->label == label) {
                std::optional<std::uint8_t> close = it->close_base;
                if (is_continue && it->repeat) {
                    close.reset();
                    if (block_bases_.size() > it->body_scope_depth) {
                        close = block_bases_[it->body_scope_depth];
                    }
                }
                model_.loop_targets[stmt.id] = {it->statement, close};
                return;
            }
        }
        diagnostic(stmt.range.begin, label ? "unknown loop label: " + *label : "loop control outside loop");
    }

    void resolve_stmt(const Stmt& stmt) {
        std::visit([&](const auto& node) {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, BreakStmt> || std::is_same_v<T, ContinueStmt>) {
                resolve_jump(stmt, node.label, std::is_same_v<T, ContinueStmt>);
            } else if constexpr (std::is_same_v<T, DoStmt>) resolve_block(*node.block);
            else if constexpr (std::is_same_v<T, WhileStmt>) {
                resolve_expr(*node.condition);
                const auto base = checked_slots(function_->next_slot, node.block->range.begin);
                loops_.push_back({stmt.id, node.label, base, false, 0}); resolve_block(*node.block); loops_.pop_back();
            } else if constexpr (std::is_same_v<T, RepeatStmt>) resolve_repeat_block(node, stmt.id);
            else if constexpr (std::is_same_v<T, IfStmt>) {
                for (const auto& branch : node.branches) { resolve_expr(*branch.condition); resolve_block(*branch.block); }
                if (node.else_block) resolve_block(*node.else_block);
            } else if constexpr (std::is_same_v<T, NumericForStmt>) {
                resolve_expr(*node.initial); resolve_expr(*node.limit); if (node.step) {
                    resolve_expr(*node.step);
                    if (const auto* number = std::get_if<NumberExpr>(&node.step->kind)) {
                        try { if (std::stod(number->text) == 0.0) diagnostic(node.step->range.begin, "numeric for step must not be zero"); }
                        catch (...) {}
                    }
                }
                const auto base = checked_slots(function_->next_slot, node.block->range.begin);
                loops_.push_back({stmt.id, node.label, base, false, 0});
                resolve_loop_block(*node.block, stmt.id, {node.name}); loops_.pop_back();
            } else if constexpr (std::is_same_v<T, GenericForStmt>) {
                for (const auto& expr : node.expressions) resolve_expr(*expr);
                const auto base = checked_slots(function_->next_slot, node.block->range.begin);
                loops_.push_back({stmt.id, node.label, base, false, 0});
                resolve_loop_block(*node.block, stmt.id, node.names); loops_.pop_back();
            } else if constexpr (std::is_same_v<T, FunctionStmt>) {
                model_.function_roots[stmt.id] = resolve_name(node.name.path.front());
                resolve_function(*node.body, node.name.method.has_value());
            } else if constexpr (std::is_same_v<T, LocalFunctionStmt>) {
                model_.declarations[stmt.id] = {allocate(node.name, stmt.range.begin)};
                resolve_function(*node.body, false);
            } else if constexpr (std::is_same_v<T, LocalDeclStmt>) {
                for (const auto& expr : node.expressions) resolve_expr(*expr);
                std::vector<std::uint8_t> slots;
                for (const auto& name : node.names) slots.push_back(allocate(name, stmt.range.begin));
                model_.declarations[stmt.id] = std::move(slots);
            } else if constexpr (std::is_same_v<T, ReturnStmt>) {
                for (const auto& expr : node.expressions) resolve_expr(*expr);
            } else if constexpr (std::is_same_v<T, AssignmentStmt>) {
                for (const auto& expr : node.expressions) resolve_expr(*expr);
                for (const auto& variable : node.variables) resolve_expr(*variable);
            } else if constexpr (std::is_same_v<T, CallStmt>) resolve_expr(*node.call);
        }, stmt.kind);
    }

    SemanticModel model_;
    std::vector<Diagnostic> diagnostics_;
    FunctionContext* function_ {nullptr};
    std::vector<LoopContext> loops_;
    std::vector<std::uint8_t> block_bases_;
};

} // namespace

ResolveResult resolve(const Ast& ast) { return Resolver().run(ast); }

} // namespace suru::front

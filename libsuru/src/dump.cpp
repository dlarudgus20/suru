#include "suru/front/dump.hpp"

#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

namespace suru::front {
namespace {

std::string yaml_quote(std::string_view input) {
    std::string out {'\''};
    for (const char ch : input) {
        if (ch == '\'') out += "''";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (static_cast<unsigned char>(ch) < 0x20U) out += '?';
        else out += ch;
    }
    out += '\'';
    return out;
}

void indent(std::ostringstream& out, std::size_t depth) { out << std::string(depth * 2, ' '); }

void header(std::ostringstream& out, std::size_t depth, std::string_view kind, NodeId id, SourceRange range) {
    indent(out, depth); out << "kind: " << yaml_quote(kind) << '\n';
    indent(out, depth); out << "id: " << id << '\n';
    indent(out, depth); out << "range:\n";
    indent(out, depth + 1); out << "begin: { line: " << range.begin.line << ", column: " << range.begin.column << " }\n";
    indent(out, depth + 1); out << "end: { line: " << range.end.line << ", column: " << range.end.column << " }\n";
}

void field(std::ostringstream& out, std::size_t depth, std::string_view key, std::string_view value) {
    indent(out, depth); out << key << ": " << yaml_quote(value) << '\n';
}

void write_expr(std::ostringstream& out, const Expr& expr, std::size_t depth);
void write_stmt(std::ostringstream& out, const Stmt& stmt, std::size_t depth);
void write_block(std::ostringstream& out, const Block& block, std::size_t depth);
void write_body(std::ostringstream& out, const FunctionBody& body, std::size_t depth);

void expr_field(std::ostringstream& out, std::size_t depth, std::string_view key, const Expr& expr) {
    indent(out, depth); out << key << ":\n";
    write_expr(out, expr, depth + 1);
}

void block_field(std::ostringstream& out, std::size_t depth, std::string_view key, const Block& block) {
    indent(out, depth); out << key << ":\n";
    write_block(out, block, depth + 1);
}

void write_expr_list(std::ostringstream& out, const std::vector<ExprPtr>& list, std::size_t depth) {
    if (list.empty()) { indent(out, depth); out << "[]\n"; return; }
    for (const auto& item : list) {
        indent(out, depth); out << "-\n";
        write_expr(out, *item, depth + 1);
    }
}

void write_string_list(std::ostringstream& out, const std::vector<std::string>& list, std::size_t depth) {
    if (list.empty()) { indent(out, depth); out << "[]\n"; return; }
    for (const auto& item : list) { indent(out, depth); out << "- " << yaml_quote(item) << '\n'; }
}

void write_body(std::ostringstream& out, const FunctionBody& body, std::size_t depth) {
    header(out, depth, "FunctionBody", body.id, body.range);
    indent(out, depth); out << "parameters:\n";
    write_string_list(out, body.parameters, depth + 1);
    field(out, depth, "vararg", body.vararg ? "true" : "false");
    block_field(out, depth, "block", *body.block);
}

void write_expr(std::ostringstream& out, const Expr& expr, std::size_t depth) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, NilExpr>) header(out, depth, "NilLiteral", expr.id, expr.range);
        else if constexpr (std::is_same_v<T, BoolExpr>) {
            header(out, depth, "BooleanLiteral", expr.id, expr.range);
            field(out, depth, "value", node.value ? "true" : "false");
        } else if constexpr (std::is_same_v<T, NumberExpr>) {
            header(out, depth, "NumeralLiteral", expr.id, expr.range); field(out, depth, "value", node.text);
        } else if constexpr (std::is_same_v<T, StringExpr>) {
            header(out, depth, "StringLiteral", expr.id, expr.range); field(out, depth, "value", node.value);
        } else if constexpr (std::is_same_v<T, VarargExpr>) header(out, depth, "VarArg", expr.id, expr.range);
        else if constexpr (std::is_same_v<T, NameExpr>) {
            header(out, depth, "Name", expr.id, expr.range); field(out, depth, "value", node.name);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            header(out, depth, "UnaryExpression", expr.id, expr.range); field(out, depth, "operator", node.op);
            expr_field(out, depth, "operand", *node.operand);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            header(out, depth, "BinaryExpression", expr.id, expr.range); field(out, depth, "operator", node.op);
            expr_field(out, depth, "left", *node.left); expr_field(out, depth, "right", *node.right);
        } else if constexpr (std::is_same_v<T, GroupExpr>) {
            header(out, depth, "GroupedExpression", expr.id, expr.range); expr_field(out, depth, "expression", *node.expression);
        } else if constexpr (std::is_same_v<T, IndexExpr>) {
            header(out, depth, "IndexExpression", expr.id, expr.range);
            expr_field(out, depth, "base", *node.object); expr_field(out, depth, "index", *node.key);
        } else if constexpr (std::is_same_v<T, FieldExpr>) {
            header(out, depth, "FieldExpression", expr.id, expr.range);
            expr_field(out, depth, "base", *node.object); field(out, depth, "name", node.name);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            header(out, depth, "FunctionCallExpression", expr.id, expr.range);
            expr_field(out, depth, "callee", *node.callee); indent(out, depth); out << "arguments:\n";
            write_expr_list(out, node.arguments, depth + 1);
        } else if constexpr (std::is_same_v<T, MethodCallExpr>) {
            header(out, depth, "MethodCallExpression", expr.id, expr.range);
            expr_field(out, depth, "base", *node.receiver); field(out, depth, "method", node.method);
            indent(out, depth); out << "arguments:\n"; write_expr_list(out, node.arguments, depth + 1);
        } else if constexpr (std::is_same_v<T, FunctionExpr>) {
            header(out, depth, "FunctionExpression", expr.id, expr.range); indent(out, depth); out << "body:\n";
            write_body(out, *node.body, depth + 1);
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
            header(out, depth, "ArrayConstructor", expr.id, expr.range); indent(out, depth); out << "elements:\n";
            write_expr_list(out, node.elements, depth + 1);
        } else if constexpr (std::is_same_v<T, TableExpr>) {
            header(out, depth, "TableConstructor", expr.id, expr.range); indent(out, depth); out << "fields:\n";
            if (node.fields.empty()) { indent(out, depth + 1); out << "[]\n"; }
            for (const TableField& table_field : node.fields) {
                indent(out, depth + 1); out << "-\n";
                header(out, depth + 2, "TableField", table_field.id, table_field.range);
                expr_field(out, depth + 2, "key", *table_field.key);
                expr_field(out, depth + 2, "value", *table_field.value);
            }
        }
    }, expr.kind);
}

void write_stmt(std::ostringstream& out, const Stmt& stmt, std::size_t depth) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, BreakStmt> || std::is_same_v<T, ContinueStmt>) {
            header(out, depth, std::is_same_v<T, BreakStmt> ? "BreakStatement" : "ContinueStatement", stmt.id, stmt.range);
            if (node.label) field(out, depth, "label", *node.label);
        } else if constexpr (std::is_same_v<T, DoStmt>) {
            header(out, depth, "DoStatement", stmt.id, stmt.range); block_field(out, depth, "block", *node.block);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            header(out, depth, "WhileStatement", stmt.id, stmt.range); if (node.label) field(out, depth, "label", *node.label);
            expr_field(out, depth, "condition", *node.condition); block_field(out, depth, "block", *node.block);
        } else if constexpr (std::is_same_v<T, RepeatStmt>) {
            header(out, depth, "RepeatStatement", stmt.id, stmt.range); if (node.label) field(out, depth, "label", *node.label);
            block_field(out, depth, "block", *node.block); expr_field(out, depth, "condition", *node.condition);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            header(out, depth, "IfStatement", stmt.id, stmt.range); indent(out, depth); out << "branches:\n";
            for (const IfBranch& branch : node.branches) {
                indent(out, depth + 1); out << "-\n"; header(out, depth + 2, "IfBranch", branch.id, branch.range);
                expr_field(out, depth + 2, "condition", *branch.condition); block_field(out, depth + 2, "block", *branch.block);
            }
            if (node.else_block) block_field(out, depth, "elseBlock", *node.else_block);
        } else if constexpr (std::is_same_v<T, NumericForStmt>) {
            header(out, depth, "NumericForStatement", stmt.id, stmt.range); if (node.label) field(out, depth, "label", *node.label);
            field(out, depth, "name", node.name); expr_field(out, depth, "initial", *node.initial);
            expr_field(out, depth, "limit", *node.limit); if (node.step) expr_field(out, depth, "step", *node.step);
            block_field(out, depth, "block", *node.block);
        } else if constexpr (std::is_same_v<T, GenericForStmt>) {
            header(out, depth, "GenericForStatement", stmt.id, stmt.range); if (node.label) field(out, depth, "label", *node.label);
            indent(out, depth); out << "names:\n"; write_string_list(out, node.names, depth + 1);
            indent(out, depth); out << "expressions:\n"; write_expr_list(out, node.expressions, depth + 1);
            block_field(out, depth, "block", *node.block);
        } else if constexpr (std::is_same_v<T, FunctionStmt>) {
            header(out, depth, "FunctionStatement", stmt.id, stmt.range); indent(out, depth); out << "path:\n";
            write_string_list(out, node.name.path, depth + 1); if (node.name.method) field(out, depth, "method", *node.name.method);
            indent(out, depth); out << "body:\n"; write_body(out, *node.body, depth + 1);
        } else if constexpr (std::is_same_v<T, LocalFunctionStmt>) {
            header(out, depth, "LocalFunctionStatement", stmt.id, stmt.range); field(out, depth, "name", node.name);
            indent(out, depth); out << "body:\n"; write_body(out, *node.body, depth + 1);
        } else if constexpr (std::is_same_v<T, LocalDeclStmt>) {
            header(out, depth, "LocalDeclaration", stmt.id, stmt.range); indent(out, depth); out << "names:\n";
            write_string_list(out, node.names, depth + 1); indent(out, depth); out << "expressions:\n";
            write_expr_list(out, node.expressions, depth + 1);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            header(out, depth, "ReturnStatement", stmt.id, stmt.range); indent(out, depth); out << "expressions:\n";
            write_expr_list(out, node.expressions, depth + 1);
        } else if constexpr (std::is_same_v<T, AssignmentStmt>) {
            header(out, depth, "AssignmentStatement", stmt.id, stmt.range); indent(out, depth); out << "variables:\n";
            write_expr_list(out, node.variables, depth + 1); indent(out, depth); out << "expressions:\n";
            write_expr_list(out, node.expressions, depth + 1);
        } else if constexpr (std::is_same_v<T, CallStmt>) {
            header(out, depth, "FunctionCallStatement", stmt.id, stmt.range); expr_field(out, depth, "call", *node.call);
        }
    }, stmt.kind);
}

void write_block(std::ostringstream& out, const Block& block, std::size_t depth) {
    header(out, depth, "Block", block.id, block.range);
    indent(out, depth); out << "statements:\n";
    if (block.statements.empty()) { indent(out, depth + 1); out << "[]\n"; return; }
    for (const auto& stmt : block.statements) {
        indent(out, depth + 1); out << "-\n";
        write_stmt(out, *stmt, depth + 2);
    }
}

} // namespace

std::string dump(const ParseTree& tree) {
    std::ostringstream out;
    write_block(out, tree.root, 0);
    out << '\n';
    return out.str();
}

} // namespace suru::front

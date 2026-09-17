#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "suru/front/token.hpp"

namespace suru::front {

using NodeId = std::uint32_t;

struct SourceRange {
    SourceLocation begin;
    SourceLocation end;
};

struct Expr;
struct Stmt;
struct Block;
struct FunctionBody;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using BlockPtr = std::unique_ptr<Block>;
using FunctionBodyPtr = std::unique_ptr<FunctionBody>;

struct NilExpr {};
struct BoolExpr { bool value; };
struct NumberExpr { std::string text; };
struct StringExpr { std::string value; };
struct VarargExpr {};
struct NameExpr { std::string name; };
struct UnaryExpr { std::string op; ExprPtr operand; };
struct BinaryExpr { std::string op; ExprPtr left; ExprPtr right; };
struct GroupExpr { ExprPtr expression; };
struct IndexExpr { ExprPtr object; ExprPtr key; };
struct FieldExpr { ExprPtr object; std::string name; };
struct CallExpr { ExprPtr callee; std::vector<ExprPtr> arguments; };
struct MethodCallExpr { ExprPtr receiver; std::string method; std::vector<ExprPtr> arguments; };
struct FunctionExpr { FunctionBodyPtr body; };
struct ArrayExpr { std::vector<ExprPtr> elements; };

struct TableField {
    NodeId id {0};
    SourceRange range {};
    ExprPtr key;
    ExprPtr value;
};

struct TableExpr { std::vector<TableField> fields; };

using ExprKind = std::variant<
    NilExpr, BoolExpr, NumberExpr, StringExpr, VarargExpr, NameExpr,
    UnaryExpr, BinaryExpr, GroupExpr, IndexExpr, FieldExpr, CallExpr,
    MethodCallExpr, FunctionExpr, ArrayExpr, TableExpr
>;

struct Expr {
    NodeId id {0};
    SourceRange range {};
    ExprKind kind;
};

struct BreakStmt { std::optional<std::string> label; };
struct ContinueStmt { std::optional<std::string> label; };
struct DoStmt { BlockPtr block; };
struct WhileStmt { std::optional<std::string> label; ExprPtr condition; BlockPtr block; };
struct RepeatStmt { std::optional<std::string> label; BlockPtr block; ExprPtr condition; };

struct IfBranch {
    NodeId id {0};
    SourceRange range {};
    ExprPtr condition;
    BlockPtr block;
};

struct IfStmt { std::vector<IfBranch> branches; BlockPtr else_block; };
struct NumericForStmt {
    std::optional<std::string> label;
    std::string name;
    ExprPtr initial;
    ExprPtr limit;
    ExprPtr step;
    BlockPtr block;
};
struct GenericForStmt {
    std::optional<std::string> label;
    std::vector<std::string> names;
    std::vector<ExprPtr> expressions;
    BlockPtr block;
};
struct FunctionName { std::vector<std::string> path; std::optional<std::string> method; };
struct FunctionStmt { FunctionName name; FunctionBodyPtr body; };
struct LocalFunctionStmt { std::string name; FunctionBodyPtr body; };
struct LocalDeclStmt { std::vector<std::string> names; std::vector<ExprPtr> expressions; };
struct ReturnStmt { std::vector<ExprPtr> expressions; };
struct AssignmentStmt { std::vector<ExprPtr> variables; std::vector<ExprPtr> expressions; };
struct CallStmt { ExprPtr call; };

using StmtKind = std::variant<
    BreakStmt, ContinueStmt, DoStmt, WhileStmt, RepeatStmt, IfStmt,
    NumericForStmt, GenericForStmt, FunctionStmt, LocalFunctionStmt,
    LocalDeclStmt, ReturnStmt, AssignmentStmt, CallStmt
>;

struct Stmt {
    NodeId id {0};
    SourceRange range {};
    StmtKind kind;
};

struct Block {
    NodeId id {0};
    SourceRange range {};
    std::vector<StmtPtr> statements;
};

struct FunctionBody {
    NodeId id {0};
    SourceRange range {};
    std::vector<std::string> parameters;
    bool vararg {false};
    BlockPtr block;
};

struct Ast {
    Block root;
};

using ParseTree = Ast;

} // namespace suru::front

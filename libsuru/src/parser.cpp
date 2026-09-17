#include "suru/front/parser.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace suru::front {
namespace {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    ParseResult run() {
        if (tokens_.empty()) tokens_.push_back(Token {TokenKind::EndOfFile, "", {}});
        else if (tokens_.back().kind != TokenKind::EndOfFile) {
            tokens_.push_back(Token {TokenKind::EndOfFile, "", {tokens_.back().range.end, tokens_.back().range.end}});
        }

        ParseResult result;
        result.tree.root = parse_root();
        result.diagnostics = std::move(diagnostics_);
        if (status_ == ParseStatus::Ok && !result.diagnostics.empty()) status_ = ParseStatus::Error;
        result.status = status_;
        return result;
    }

private:
    NodeId next_id() { return next_id_++; }
    SourceRange range(SourceLocation location) const {
        SourceLocation end = index_ == 0 ? location : previous().range.end;
        if (end.line < location.line || (end.line == location.line && end.column < location.column)) end = location;
        return {location, end};
    }

    template <typename T>
    ExprPtr expression(SourceLocation location, T kind) {
        return std::make_unique<Expr>(Expr {next_id(), range(location), ExprKind {std::move(kind)}});
    }

    template <typename T>
    StmtPtr statement(SourceLocation location, T kind) {
        return std::make_unique<Stmt>(Stmt {next_id(), range(location), StmtKind {std::move(kind)}});
    }

    Block parse_root() {
        Block block = parse_block();
        if (!failed_ && !at_end()) error_here("unexpected token after block");
        return block;
    }

    Block parse_block() {
        const SourceLocation location = current().range.begin;
        Block block {next_id(), {location, location}, {}};
        while (!at_end() && !is_terminator(current().kind)) {
            if (match(TokenKind::Semicolon)) continue;
            if (current().kind == TokenKind::KwReturn) {
                block.statements.push_back(parse_return_statement());
                while (match(TokenKind::Semicolon)) {}
                break;
            }
            block.statements.push_back(parse_statement());
            if (failed_) break;
            while (match(TokenKind::Semicolon)) {}
        }
        if (block.statements.empty()) block.range = {current().range.begin, current().range.begin};
        else block.range = {block.statements.front()->range.begin, block.statements.back()->range.end};
        return block;
    }

    BlockPtr block_ptr() { return std::make_unique<Block>(parse_block()); }

    StmtPtr parse_statement() {
        switch (current().kind) {
            case TokenKind::KwBreak: return parse_break_statement();
            case TokenKind::KwContinue: return parse_continue_statement();
            case TokenKind::KwDo: return parse_do_statement();
            case TokenKind::KwWhile: return parse_while_statement({});
            case TokenKind::KwRepeat: return parse_repeat_statement({});
            case TokenKind::KwIf: return parse_if_statement();
            case TokenKind::KwFor: return parse_for_statement({});
            case TokenKind::KwFunction: return parse_function_statement();
            case TokenKind::KwLocal: return parse_local_statement();
            default:
                if (current().kind == TokenKind::Identifier
                    && peek(1).kind == TokenKind::Colon
                    && (peek(2).kind == TokenKind::KwWhile
                        || peek(2).kind == TokenKind::KwRepeat
                        || peek(2).kind == TokenKind::KwFor)) {
                    const SourceLocation begin = current().range.begin;
                    std::string label = advance().lexeme;
                    advance();
                    StmtPtr loop;
                    if (current().kind == TokenKind::KwWhile) loop = parse_while_statement(std::move(label));
                    else if (current().kind == TokenKind::KwRepeat) loop = parse_repeat_statement(std::move(label));
                    else loop = parse_for_statement(std::move(label));
                    loop->range.begin = begin;
                    return loop;
                }
                return parse_assignment_or_call_statement();
        }
    }

    StmtPtr parse_break_statement() {
        const Token keyword = advance();
        std::optional<std::string> label;
        if (current().kind == TokenKind::Identifier) label = advance().lexeme;
        return statement(keyword.range.begin, BreakStmt {std::move(label)});
    }

    StmtPtr parse_continue_statement() {
        const Token keyword = advance();
        std::optional<std::string> label;
        if (current().kind == TokenKind::Identifier) label = advance().lexeme;
        return statement(keyword.range.begin, ContinueStmt {std::move(label)});
    }

    StmtPtr parse_do_statement() {
        const Token keyword = advance();
        BlockPtr block = block_ptr();
        consume(TokenKind::KwEnd, "expected 'end'");
        return statement(keyword.range.begin, DoStmt {std::move(block)});
    }

    StmtPtr parse_while_statement(std::optional<std::string> label) {
        const Token keyword = consume(TokenKind::KwWhile, "expected 'while'");
        ExprPtr condition = parse_expression();
        consume(TokenKind::KwDo, "expected 'do'");
        BlockPtr block = block_ptr();
        consume(TokenKind::KwEnd, "expected 'end'");
        return statement(keyword.range.begin, WhileStmt {std::move(label), std::move(condition), std::move(block)});
    }

    StmtPtr parse_repeat_statement(std::optional<std::string> label) {
        const Token keyword = consume(TokenKind::KwRepeat, "expected 'repeat'");
        BlockPtr block = block_ptr();
        consume(TokenKind::KwUntil, "expected 'until'");
        ExprPtr condition = parse_expression();
        return statement(keyword.range.begin, RepeatStmt {std::move(label), std::move(block), std::move(condition)});
    }

    StmtPtr parse_if_statement() {
        const Token keyword = consume(TokenKind::KwIf, "expected 'if'");
        std::vector<IfBranch> branches;
        do {
            const SourceLocation location = previous().range.begin;
            ExprPtr condition = parse_expression();
            consume(TokenKind::KwThen, "expected 'then'");
            BlockPtr block = block_ptr();
            branches.push_back(IfBranch {next_id(), range(location), std::move(condition), std::move(block)});
        } while (match(TokenKind::KwElseIf));

        BlockPtr else_block;
        if (match(TokenKind::KwElse)) else_block = block_ptr();
        consume(TokenKind::KwEnd, "expected 'end'");
        return statement(keyword.range.begin, IfStmt {std::move(branches), std::move(else_block)});
    }

    StmtPtr parse_for_statement(std::optional<std::string> label) {
        const Token keyword = consume(TokenKind::KwFor, "expected 'for'");
        const Token first = consume(TokenKind::Identifier, "expected loop variable");
        if (match(TokenKind::Assign)) {
            ExprPtr initial = parse_expression();
            consume(TokenKind::Comma, "expected ','");
            ExprPtr limit = parse_expression();
            ExprPtr step;
            if (match(TokenKind::Comma)) step = parse_expression();
            consume(TokenKind::KwDo, "expected 'do'");
            BlockPtr block = block_ptr();
            consume(TokenKind::KwEnd, "expected 'end'");
            return statement(keyword.range.begin, NumericForStmt {
                std::move(label), first.lexeme, std::move(initial), std::move(limit),
                std::move(step), std::move(block)
            });
        }

        std::vector<std::string> names {first.lexeme};
        while (match(TokenKind::Comma)) names.push_back(consume(TokenKind::Identifier, "expected loop variable").lexeme);
        consume(TokenKind::KwIn, "expected 'in'");
        auto expressions = parse_expression_list();
        consume(TokenKind::KwDo, "expected 'do'");
        BlockPtr block = block_ptr();
        consume(TokenKind::KwEnd, "expected 'end'");
        return statement(keyword.range.begin, GenericForStmt {
            std::move(label), std::move(names), std::move(expressions), std::move(block)
        });
    }

    FunctionName parse_function_name() {
        FunctionName name;
        name.path.push_back(consume(TokenKind::Identifier, "expected function name").lexeme);
        while (match(TokenKind::Dot)) name.path.push_back(consume(TokenKind::Identifier, "expected identifier").lexeme);
        if (match(TokenKind::Colon)) name.method = consume(TokenKind::Identifier, "expected method name").lexeme;
        return name;
    }

    FunctionBodyPtr parse_function_body() {
        const SourceLocation location = current().range.begin;
        consume(TokenKind::LParen, "expected '('");
        std::vector<std::string> parameters;
        bool vararg = false;
        if (current().kind != TokenKind::RParen) {
            if (match(TokenKind::VarArg)) {
                vararg = true;
            } else {
                parameters.push_back(consume(TokenKind::Identifier, "expected parameter name").lexeme);
                while (match(TokenKind::Comma)) {
                    if (match(TokenKind::VarArg)) { vararg = true; break; }
                    parameters.push_back(consume(TokenKind::Identifier, "expected parameter name").lexeme);
                }
            }
        }
        consume(TokenKind::RParen, "expected ')'");
        BlockPtr block = block_ptr();
        consume(TokenKind::KwEnd, "expected 'end'");
        return std::make_unique<FunctionBody>(FunctionBody {
            next_id(), range(location), std::move(parameters), vararg, std::move(block)
        });
    }

    StmtPtr parse_function_statement() {
        const Token keyword = advance();
        FunctionName name = parse_function_name();
        FunctionBodyPtr body = parse_function_body();
        return statement(keyword.range.begin, FunctionStmt {std::move(name), std::move(body)});
    }

    StmtPtr parse_local_statement() {
        const Token keyword = advance();
        if (match(TokenKind::KwFunction)) {
            const Token name = consume(TokenKind::Identifier, "expected function name");
            return statement(keyword.range.begin, LocalFunctionStmt {name.lexeme, parse_function_body()});
        }
        std::vector<std::string> names;
        names.push_back(consume(TokenKind::Identifier, "expected local name").lexeme);
        while (match(TokenKind::Comma)) names.push_back(consume(TokenKind::Identifier, "expected local name").lexeme);
        std::vector<ExprPtr> expressions;
        if (match(TokenKind::Assign)) expressions = parse_expression_list();
        return statement(keyword.range.begin, LocalDeclStmt {std::move(names), std::move(expressions)});
    }

    StmtPtr parse_return_statement() {
        const Token keyword = advance();
        std::vector<ExprPtr> expressions;
        if (current().kind != TokenKind::Semicolon && !is_terminator(current().kind) && !at_end()) {
            expressions = parse_expression_list();
        }
        return statement(keyword.range.begin, ReturnStmt {std::move(expressions)});
    }

    StmtPtr parse_assignment_or_call_statement() {
        ExprPtr left = parse_prefix_expression();
        if (failed_) return statement(current().range.begin, CallStmt {std::move(left)});
        if (is_call(*left) && current().kind != TokenKind::Assign && current().kind != TokenKind::Comma) {
            const SourceLocation location = left->range.begin;
            return statement(location, CallStmt {std::move(left)});
        }
        if (!is_variable(*left)) {
            const SourceLocation location = left->range.begin;
            error_at(location, "expected assignment or function call statement");
            return statement(location, CallStmt {std::move(left)});
        }
        const SourceLocation location = left->range.begin;
        std::vector<ExprPtr> variables;
        variables.push_back(std::move(left));
        while (match(TokenKind::Comma)) {
            ExprPtr variable = parse_prefix_expression();
            if (!is_variable(*variable)) { error_at(variable->range.begin, "expected variable in assignment"); break; }
            variables.push_back(std::move(variable));
        }
        consume(TokenKind::Assign, "expected '='");
        auto expressions = parse_expression_list();
        return statement(location, AssignmentStmt {std::move(variables), std::move(expressions)});
    }

    std::vector<ExprPtr> parse_expression_list() {
        std::vector<ExprPtr> expressions;
        expressions.push_back(parse_expression());
        while (match(TokenKind::Comma)) expressions.push_back(parse_expression());
        return expressions;
    }

    ExprPtr parse_expression() { return parse_subexpression(0); }

    ExprPtr parse_subexpression(int min_priority) {
        ExprPtr left;
        if (is_unary(current().kind)) {
            const Token op = advance();
            left = expression(op.range.begin, UnaryExpr {op.lexeme, parse_subexpression(11)});
        } else left = parse_simple_expression();
        while (!failed_) {
            int lhs_priority = 0;
            int rhs_priority = 0;
            if (!binary_priority(current().kind, lhs_priority, rhs_priority) || lhs_priority <= min_priority) break;
            const Token op = advance();
            const SourceLocation begin = left->range.begin;
            left = expression(begin, BinaryExpr {op.lexeme, std::move(left), parse_subexpression(rhs_priority)});
        }
        return left;
    }

    ExprPtr unterminated_string() {
        const Token token = advance();
        if (!failed_) {
            diagnostics_.push_back({token.range.begin, "unterminated string"});
            status_ = ParseStatus::Incomplete;
            failed_ = true;
        }
        return expression(token.range.begin, StringExpr {token.lexeme});
    }

    ExprPtr parse_simple_expression() {
        const Token token = current();
        switch (token.kind) {
            case TokenKind::KwNil: advance(); return expression(token.range.begin, NilExpr {});
            case TokenKind::KwFalse: advance(); return expression(token.range.begin, BoolExpr {false});
            case TokenKind::KwTrue: advance(); return expression(token.range.begin, BoolExpr {true});
            case TokenKind::Numeral: advance(); return expression(token.range.begin, NumberExpr {token.lexeme});
            case TokenKind::String: advance(); return expression(token.range.begin, StringExpr {token.lexeme});
            case TokenKind::UnterminatedString: return unterminated_string();
            case TokenKind::VarArg: advance(); return expression(token.range.begin, VarargExpr {});
            case TokenKind::KwFunction: advance(); return expression(token.range.begin, FunctionExpr {parse_function_body()});
            case TokenKind::LBrace: return parse_table_constructor();
            case TokenKind::LBracket: return parse_array_constructor();
            default: return parse_prefix_expression();
        }
    }

    ExprPtr parse_array_constructor() {
        const Token open = advance();
        std::vector<ExprPtr> elements;
        if (current().kind != TokenKind::RBracket) {
            elements = parse_expression_list();
            match(TokenKind::Comma);
        }
        consume(TokenKind::RBracket, "expected ']'");
        return expression(open.range.begin, ArrayExpr {std::move(elements)});
    }

    ExprPtr literal_table_key() {
        if (current().kind == TokenKind::UnterminatedString) return unterminated_string();
        const Token token = advance();
        switch (token.kind) {
            case TokenKind::Identifier: case TokenKind::String:
                return expression(token.range.begin, StringExpr {token.lexeme});
            case TokenKind::Numeral: return expression(token.range.begin, NumberExpr {token.lexeme});
            case TokenKind::KwTrue: return expression(token.range.begin, BoolExpr {true});
            case TokenKind::KwFalse: return expression(token.range.begin, BoolExpr {false});
            default: break;
        }
        error_at(token.range.begin, "expected table key");
        return expression(token.range.begin, NilExpr {});
    }

    ExprPtr parse_table_constructor() {
        const Token open = advance();
        std::vector<TableField> fields;
        while (!failed_ && current().kind != TokenKind::RBrace) {
            const SourceLocation location = current().range.begin;
            ExprPtr key;
            if (match(TokenKind::LParen)) {
                key = parse_expression();
                consume(TokenKind::RParen, "expected ')'");
            } else if (current().kind == TokenKind::Identifier || current().kind == TokenKind::String
                || current().kind == TokenKind::UnterminatedString
                || current().kind == TokenKind::Numeral || current().kind == TokenKind::KwTrue
                || current().kind == TokenKind::KwFalse) {
                key = literal_table_key();
            } else {
                error_here("expected explicit table key");
                break;
            }
            consume(TokenKind::Assign, "expected '='");
            ExprPtr value = parse_expression();
            fields.push_back(TableField {next_id(), range(location), std::move(key), std::move(value)});
            if (!match(TokenKind::Comma) && !match(TokenKind::Semicolon)) break;
        }
        consume(TokenKind::RBrace, "expected '}'");
        return expression(open.range.begin, TableExpr {std::move(fields)});
    }

    ExprPtr parse_prefix_expression() {
        ExprPtr result;
        if (match(TokenKind::Identifier)) {
            const Token name = previous();
            result = expression(name.range.begin, NameExpr {name.lexeme});
        } else if (match(TokenKind::LParen)) {
            const Token open = previous();
            ExprPtr inner = parse_expression();
            consume(TokenKind::RParen, "expected ')'");
            result = expression(open.range.begin, GroupExpr {std::move(inner)});
        } else {
            const SourceLocation location = current().range.begin;
            error_here("expected expression");
            return expression(location, NilExpr {});
        }
        while (!failed_) {
            const SourceLocation begin = result->range.begin;
            if (match(TokenKind::LBracket)) {
                ExprPtr key = parse_expression();
                consume(TokenKind::RBracket, "expected ']'");
                result = expression(begin, IndexExpr {std::move(result), std::move(key)});
            } else if (match(TokenKind::Dot)) {
                const Token name = consume(TokenKind::Identifier, "expected field name");
                result = expression(begin, FieldExpr {std::move(result), name.lexeme});
            } else if (match(TokenKind::Colon)) {
                const Token method = consume(TokenKind::Identifier, "expected method name");
                result = expression(begin, MethodCallExpr {std::move(result), method.lexeme, parse_arguments()});
            } else if (starts_arguments(current().kind)) {
                result = expression(begin, CallExpr {std::move(result), parse_arguments()});
            } else break;
        }
        return result;
    }

    std::vector<ExprPtr> parse_arguments() {
        std::vector<ExprPtr> arguments;
        if (match(TokenKind::LParen)) {
            if (current().kind != TokenKind::RParen) arguments = parse_expression_list();
            consume(TokenKind::RParen, "expected ')'");
        } else if (current().kind == TokenKind::LBrace) arguments.push_back(parse_table_constructor());
        else if (current().kind == TokenKind::String || current().kind == TokenKind::UnterminatedString) {
            arguments.push_back(parse_simple_expression());
        }
        else error_here("expected function arguments");
        return arguments;
    }

    static bool starts_arguments(TokenKind kind) {
        return kind == TokenKind::LParen || kind == TokenKind::LBrace
            || kind == TokenKind::String || kind == TokenKind::UnterminatedString;
    }
    static bool is_variable(const Expr& expr) {
        return std::holds_alternative<NameExpr>(expr.kind) || std::holds_alternative<IndexExpr>(expr.kind)
            || std::holds_alternative<FieldExpr>(expr.kind);
    }
    static bool is_call(const Expr& expr) {
        return std::holds_alternative<CallExpr>(expr.kind) || std::holds_alternative<MethodCallExpr>(expr.kind);
    }
    static bool is_unary(TokenKind kind) {
        return kind == TokenKind::Minus || kind == TokenKind::KwNot || kind == TokenKind::Hash || kind == TokenKind::Tilde;
    }
    static bool binary_priority(TokenKind kind, int& left, int& right) {
        switch (kind) {
            case TokenKind::KwOr: left = 1; right = 1; return true;
            case TokenKind::KwAnd: left = 2; right = 2; return true;
            case TokenKind::Less: case TokenKind::LessEq: case TokenKind::Greater:
            case TokenKind::GreaterEq: case TokenKind::EqEq: case TokenKind::NotEq:
                left = 3; right = 3; return true;
            case TokenKind::Pipe: left = 4; right = 4; return true;
            case TokenKind::Caret: left = 5; right = 5; return true;
            case TokenKind::Amp: left = 6; right = 6; return true;
            case TokenKind::ShiftLeft: case TokenKind::ShiftRight: left = 7; right = 7; return true;
            case TokenKind::DotDot: left = 8; right = 7; return true;
            case TokenKind::Plus: case TokenKind::Minus: left = 9; right = 9; return true;
            case TokenKind::Star: case TokenKind::Slash: case TokenKind::SlashSlash:
            case TokenKind::Percent: left = 10; right = 10; return true;
            case TokenKind::Pow: left = 12; right = 11; return true;
            default: return false;
        }
    }
    bool is_terminator(TokenKind kind) const {
        return kind == TokenKind::KwEnd || kind == TokenKind::KwElse || kind == TokenKind::KwElseIf || kind == TokenKind::KwUntil;
    }

    bool at_end() const { return current().kind == TokenKind::EndOfFile; }
    const Token& current() const { return tokens_[index_]; }
    const Token& previous() const { return tokens_[index_ - 1]; }
    const Token& peek(std::size_t offset) const {
        const std::size_t position = index_ + offset;
        return position < tokens_.size() ? tokens_[position] : tokens_.back();
    }
    Token advance() { const Token token = current(); if (!at_end()) ++index_; return token; }
    bool match(TokenKind kind) { if (current().kind != kind) return false; advance(); return true; }
    Token consume(TokenKind kind, const std::string& message) {
        if (current().kind == kind) return advance();
        if (at_end()) incomplete_here(); else error_here(message);
        return Token {kind, "", current().range};
    }
    void error_here(const std::string& message) { if (at_end()) incomplete_here(); else error_at(current().range.begin, message); }
    void incomplete_here() {
        if (!failed_) diagnostics_.push_back({current().range.begin, "unexpected end of file"});
        status_ = ParseStatus::Incomplete;
        failed_ = true;
    }
    void error_at(SourceLocation location, const std::string& message) {
        if (!failed_) diagnostics_.push_back({location, message});
        if (status_ == ParseStatus::Ok) status_ = ParseStatus::Error;
        failed_ = true;
    }

    std::vector<Token> tokens_;
    std::size_t index_ {0};
    NodeId next_id_ {1};
    bool failed_ {false};
    ParseStatus status_ {ParseStatus::Ok};
    std::vector<Diagnostic> diagnostics_;
};

} // namespace

ParseResult parse_tokens(std::vector<Token> tokens) { return Parser(std::move(tokens)).run(); }

} // namespace suru::front

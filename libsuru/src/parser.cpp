
#include "suru/front/parser.hpp"
#include "suru/front/lexer.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace suru::front {
namespace {

ParseNode make_node(std::string kind, SourceLocation location) {
    ParseNode node;
    node.kind = std::move(kind);
    node.location = location;
    return node;
}

void add_attr(ParseNode& node, std::string key, std::string value) {
    node.attributes.push_back({std::move(key), std::move(value)});
}

void add_node(ParseNode& node, std::string key, ParseNode child) {
    node.nodes.push_back({std::move(key), std::move(child)});
}

void add_list(ParseNode& node, std::string key, std::vector<ParseNode> list) {
    node.lists.push_back({std::move(key), std::move(list)});
}

} // namespace

class Parser::Impl {
public:
    explicit Impl(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    ParseResult run() {
        ParseResult result;
        result.tree.root = parse_root();
        result.diagnostics = std::move(diagnostics_);
        return result;
    }

private:
    ParseNode parse_root() {
        ParseNode block = parse_block();
        if (!failed_ && !at_end()) {
            error_here("unexpected token after block");
        }
        return block;
    }

    ParseNode parse_block() {
        ParseNode block = make_node("Block", current().location);
        std::vector<ParseNode> statements;
        while (!at_end() && !is_terminator(current().kind)) {
            if (current().kind == TokenKind::Semicolon) {
                advance();
                continue;
            }
            if (current().kind == TokenKind::KwReturn) {
                statements.push_back(parse_return_statement());
                while (match(TokenKind::Semicolon)) {
                }
                break;
            }
            statements.push_back(parse_statement());
            if (failed_) {
                break;
            }
            while (match(TokenKind::Semicolon)) {
            }
        }
        add_list(block, "statements", std::move(statements));
        return block;
    }

    ParseNode parse_statement() {
        switch (current().kind) {
            case TokenKind::KwBreak: return parse_break_statement();
            case TokenKind::KwGoto: return parse_goto_statement();
            case TokenKind::KwDo: return parse_do_statement();
            case TokenKind::KwWhile: return parse_while_statement();
            case TokenKind::KwRepeat: return parse_repeat_statement();
            case TokenKind::KwIf: return parse_if_statement();
            case TokenKind::KwFor: return parse_for_statement();
            case TokenKind::KwFunction: return parse_function_statement();
            case TokenKind::KwLocal: return parse_local_statement();
            case TokenKind::ColonColon: return parse_label_statement();
            default: return parse_assignment_or_call_statement();
        }
    }

    ParseNode parse_break_statement() {
        ParseNode node = make_node("BreakStatement", current().location);
        consume(TokenKind::KwBreak, "expected 'break'");
        return node;
    }

    ParseNode parse_goto_statement() {
        ParseNode node = make_node("GotoStatement", current().location);
        consume(TokenKind::KwGoto, "expected 'goto'");
        Token label = consume(TokenKind::Identifier, "expected label name");
        add_attr(node, "label", label.lexeme);
        return node;
    }

    ParseNode parse_do_statement() {
        ParseNode node = make_node("DoStatement", current().location);
        consume(TokenKind::KwDo, "expected 'do'");
        add_node(node, "block", parse_block());
        consume(TokenKind::KwEnd, "expected 'end'");
        return node;
    }

    ParseNode parse_while_statement() {
        ParseNode node = make_node("WhileStatement", current().location);
        consume(TokenKind::KwWhile, "expected 'while'");
        add_node(node, "condition", parse_expression());
        consume(TokenKind::KwDo, "expected 'do'");
        add_node(node, "block", parse_block());
        consume(TokenKind::KwEnd, "expected 'end'");
        return node;
    }

    ParseNode parse_repeat_statement() {
        ParseNode node = make_node("RepeatStatement", current().location);
        consume(TokenKind::KwRepeat, "expected 'repeat'");
        add_node(node, "block", parse_block());
        consume(TokenKind::KwUntil, "expected 'until'");
        add_node(node, "condition", parse_expression());
        return node;
    }

    ParseNode parse_if_statement() {
        ParseNode node = make_node("IfStatement", current().location);
        consume(TokenKind::KwIf, "expected 'if'");

        std::vector<ParseNode> branches;
        ParseNode branch = make_node("IfBranch", current().location);
        add_node(branch, "condition", parse_expression());
        consume(TokenKind::KwThen, "expected 'then'");
        add_node(branch, "block", parse_block());
        branches.push_back(std::move(branch));

        while (match(TokenKind::KwElseIf)) {
            ParseNode elseif_branch = make_node("ElseIfBranch", previous().location);
            add_node(elseif_branch, "condition", parse_expression());
            consume(TokenKind::KwThen, "expected 'then'");
            add_node(elseif_branch, "block", parse_block());
            branches.push_back(std::move(elseif_branch));
        }

        add_list(node, "branches", std::move(branches));

        if (match(TokenKind::KwElse)) {
            add_node(node, "elseBlock", parse_block());
        }
        consume(TokenKind::KwEnd, "expected 'end'");
        return node;
    }

    ParseNode parse_for_statement() {
        consume(TokenKind::KwFor, "expected 'for'");
        Token first_name = consume(TokenKind::Identifier, "expected loop variable");

        if (match(TokenKind::Assign)) {
            ParseNode node = make_node("NumericForStatement", first_name.location);
            add_attr(node, "name", first_name.lexeme);
            add_node(node, "initial", parse_expression());
            consume(TokenKind::Comma, "expected ','");
            add_node(node, "limit", parse_expression());
            if (match(TokenKind::Comma)) {
                add_node(node, "step", parse_expression());
            }
            consume(TokenKind::KwDo, "expected 'do'");
            add_node(node, "block", parse_block());
            consume(TokenKind::KwEnd, "expected 'end'");
            return node;
        }

        ParseNode node = make_node("GenericForStatement", first_name.location);
        std::vector<ParseNode> names;
        ParseNode first = make_node("Name", first_name.location);
        add_attr(first, "value", first_name.lexeme);
        names.push_back(std::move(first));
        while (match(TokenKind::Comma)) {
            Token n = consume(TokenKind::Identifier, "expected loop variable");
            ParseNode name = make_node("Name", n.location);
            add_attr(name, "value", n.lexeme);
            names.push_back(std::move(name));
        }
        consume(TokenKind::KwIn, "expected 'in'");
        add_list(node, "names", std::move(names));
        add_list(node, "expressions", parse_expression_list());
        consume(TokenKind::KwDo, "expected 'do'");
        add_node(node, "block", parse_block());
        consume(TokenKind::KwEnd, "expected 'end'");
        return node;
    }

    ParseNode parse_function_statement() {
        ParseNode node = make_node("FunctionStatement", current().location);
        consume(TokenKind::KwFunction, "expected 'function'");
        add_node(node, "name", parse_function_name());
        add_node(node, "body", parse_function_body());
        return node;
    }

    ParseNode parse_local_statement() {
        consume(TokenKind::KwLocal, "expected 'local'");
        if (match(TokenKind::KwFunction)) {
            ParseNode node = make_node("LocalFunctionStatement", previous().location);
            Token name = consume(TokenKind::Identifier, "expected function name");
            add_attr(node, "name", name.lexeme);
            add_node(node, "body", parse_function_body());
            return node;
        }

        ParseNode node = make_node("LocalDeclaration", previous().location);
        add_list(node, "names", parse_attribute_name_list());
        if (match(TokenKind::Assign)) {
            add_list(node, "expressions", parse_expression_list());
        }
        return node;
    }

    ParseNode parse_label_statement() {
        ParseNode node = make_node("LabelStatement", current().location);
        consume(TokenKind::ColonColon, "expected '::'");
        Token label = consume(TokenKind::Identifier, "expected label name");
        consume(TokenKind::ColonColon, "expected '::'");
        add_attr(node, "label", label.lexeme);
        return node;
    }

    ParseNode parse_return_statement() {
        ParseNode node = make_node("ReturnStatement", current().location);
        consume(TokenKind::KwReturn, "expected 'return'");
        if (current().kind != TokenKind::Semicolon && !is_terminator(current().kind) && current().kind != TokenKind::EndOfFile) {
            add_list(node, "expressions", parse_expression_list());
        }
        return node;
    }

    ParseNode parse_assignment_or_call_statement() {
        ParseNode left = parse_prefix_expression();
        if (failed_) {
            return make_node("Error", current().location);
        }

        if (is_call_node(left.kind) && current().kind != TokenKind::Assign && current().kind != TokenKind::Comma) {
            ParseNode node = make_node("FunctionCallStatement", left.location);
            add_node(node, "call", std::move(left));
            return node;
        }

        if (!is_var_node(left.kind)) {
            error_at(left.location, "expected assignment or function call statement");
            return make_node("Error", left.location);
        }

        ParseNode node = make_node("AssignmentStatement", left.location);
        std::vector<ParseNode> vars;
        vars.push_back(std::move(left));
        while (match(TokenKind::Comma)) {
            ParseNode var = parse_prefix_expression();
            if (!is_var_node(var.kind)) {
                error_at(var.location, "expected variable in assignment");
                return make_node("Error", var.location);
            }
            vars.push_back(std::move(var));
        }
        consume(TokenKind::Assign, "expected '='");
        add_list(node, "variables", std::move(vars));
        add_list(node, "expressions", parse_expression_list());
        return node;
    }

    ParseNode parse_function_name() {
        Token first = consume(TokenKind::Identifier, "expected function name");
        ParseNode node = make_node("FunctionName", first.location);
        std::string path = first.lexeme;
        while (match(TokenKind::Dot)) {
            Token part = consume(TokenKind::Identifier, "expected identifier");
            path += "." + part.lexeme;
        }
        add_attr(node, "path", path);
        if (match(TokenKind::Colon)) {
            Token method = consume(TokenKind::Identifier, "expected method name");
            add_attr(node, "method", method.lexeme);
        }
        return node;
    }

    ParseNode parse_function_body() {
        ParseNode body = make_node("FunctionBody", current().location);
        consume(TokenKind::LParen, "expected '('");
        bool vararg = false;
        std::vector<ParseNode> params;
        if (current().kind != TokenKind::RParen) {
            parse_parameters(params, vararg);
        }
        consume(TokenKind::RParen, "expected ')'");
        add_list(body, "parameters", std::move(params));
        add_attr(body, "vararg", vararg ? "true" : "false");
        add_node(body, "block", parse_block());
        consume(TokenKind::KwEnd, "expected 'end'");
        return body;
    }

    void parse_parameters(std::vector<ParseNode>& params, bool& vararg) {
        if (match(TokenKind::VarArg)) {
            vararg = true;
            return;
        }
        Token p = consume(TokenKind::Identifier, "expected parameter name");
        ParseNode param = make_node("Parameter", p.location);
        add_attr(param, "name", p.lexeme);
        params.push_back(std::move(param));
        while (match(TokenKind::Comma)) {
            if (match(TokenKind::VarArg)) {
                vararg = true;
                return;
            }
            Token x = consume(TokenKind::Identifier, "expected parameter name");
            ParseNode pn = make_node("Parameter", x.location);
            add_attr(pn, "name", x.lexeme);
            params.push_back(std::move(pn));
        }
    }

    std::vector<ParseNode> parse_attribute_name_list() {
        std::vector<ParseNode> names;
        do {
            Token n = consume(TokenKind::Identifier, "expected local name");
            ParseNode node = make_node("LocalName", n.location);
            add_attr(node, "name", n.lexeme);
            if (match(TokenKind::Less)) {
                Token attr = consume(TokenKind::Identifier, "expected attribute name");
                consume(TokenKind::Greater, "expected '>'");
                add_attr(node, "attribute", attr.lexeme);
            }
            names.push_back(std::move(node));
        } while (match(TokenKind::Comma));
        return names;
    }

    std::vector<ParseNode> parse_expression_list() {
        std::vector<ParseNode> expressions;
        expressions.push_back(parse_expression());
        while (match(TokenKind::Comma)) {
            expressions.push_back(parse_expression());
        }
        return expressions;
    }

    ParseNode parse_expression() {
        return parse_subexpression(0);
    }

    ParseNode parse_subexpression(int min_priority) {
        ParseNode lhs;
        if (is_unary(current().kind)) {
            Token op = advance();
            ParseNode unary = make_node("UnaryExpression", op.location);
            add_attr(unary, "operator", op.lexeme);
            add_node(unary, "operand", parse_subexpression(11));
            lhs = std::move(unary);
        } else {
            lhs = parse_simple_expression();
        }

        while (!failed_) {
            int left = 0;
            int right = 0;
            if (!binary_priority(current().kind, left, right) || left <= min_priority) {
                break;
            }
            Token op = advance();
            ParseNode rhs = parse_subexpression(right);
            ParseNode expr = make_node("BinaryExpression", op.location);
            add_attr(expr, "operator", op.lexeme);
            add_node(expr, "left", std::move(lhs));
            add_node(expr, "right", std::move(rhs));
            lhs = std::move(expr);
        }
        return lhs;
    }

    ParseNode parse_simple_expression() {
        switch (current().kind) {
            case TokenKind::KwNil: {
                Token t = advance();
                return make_node("NilLiteral", t.location);
            }
            case TokenKind::KwFalse: {
                Token t = advance();
                ParseNode n = make_node("BooleanLiteral", t.location);
                add_attr(n, "value", "false");
                return n;
            }
            case TokenKind::KwTrue: {
                Token t = advance();
                ParseNode n = make_node("BooleanLiteral", t.location);
                add_attr(n, "value", "true");
                return n;
            }
            case TokenKind::Numeral: {
                Token t = advance();
                ParseNode n = make_node("NumeralLiteral", t.location);
                add_attr(n, "value", t.lexeme);
                return n;
            }
            case TokenKind::String: {
                Token t = advance();
                ParseNode n = make_node("StringLiteral", t.location);
                add_attr(n, "value", t.lexeme);
                return n;
            }
            case TokenKind::VarArg: {
                Token t = advance();
                return make_node("VarArg", t.location);
            }
            case TokenKind::KwFunction: {
                Token t = advance();
                ParseNode n = make_node("FunctionExpression", t.location);
                add_node(n, "body", parse_function_body());
                return n;
            }
            case TokenKind::LBrace:
                return parse_table_constructor();
            default:
                return parse_prefix_expression();
        }
    }

    ParseNode parse_table_constructor() {
        ParseNode table = make_node("TableConstructor", current().location);
        consume(TokenKind::LBrace, "expected '{'");
        std::vector<ParseNode> fields;
        if (current().kind != TokenKind::RBrace) {
            fields.push_back(parse_field());
            while (match(TokenKind::Comma) || match(TokenKind::Semicolon)) {
                if (current().kind == TokenKind::RBrace) {
                    break;
                }
                fields.push_back(parse_field());
            }
        }
        consume(TokenKind::RBrace, "expected '}'");
        add_list(table, "fields", std::move(fields));
        return table;
    }

    ParseNode parse_field() {
        if (match(TokenKind::LBracket)) {
            ParseNode field = make_node("TableField", previous().location);
            add_attr(field, "fieldKind", "computed");
            add_node(field, "key", parse_expression());
            consume(TokenKind::RBracket, "expected ']'");
            consume(TokenKind::Assign, "expected '='");
            add_node(field, "value", parse_expression());
            return field;
        }
        if (current().kind == TokenKind::Identifier && peek(1).kind == TokenKind::Assign) {
            Token k = advance();
            consume(TokenKind::Assign, "expected '='");
            ParseNode field = make_node("TableField", k.location);
            add_attr(field, "fieldKind", "named");
            add_attr(field, "name", k.lexeme);
            add_node(field, "value", parse_expression());
            return field;
        }
        ParseNode field = make_node("TableField", current().location);
        add_attr(field, "fieldKind", "array");
        add_node(field, "value", parse_expression());
        return field;
    }

    ParseNode parse_prefix_expression() {
        ParseNode expr;
        if (match(TokenKind::Identifier)) {
            Token name = previous();
            expr = make_node("Name", name.location);
            add_attr(expr, "value", name.lexeme);
        } else if (match(TokenKind::LParen)) {
            ParseNode grouped = make_node("GroupedExpression", previous().location);
            add_node(grouped, "expression", parse_expression());
            consume(TokenKind::RParen, "expected ')'");
            expr = std::move(grouped);
        } else {
            error_here("expected expression");
            return make_node("Error", current().location);
        }

        while (!failed_) {
            if (match(TokenKind::LBracket)) {
                ParseNode index = make_node("IndexExpression", previous().location);
                add_node(index, "base", std::move(expr));
                add_node(index, "index", parse_expression());
                consume(TokenKind::RBracket, "expected ']'");
                expr = std::move(index);
                continue;
            }
            if (match(TokenKind::Dot)) {
                Token field_name = consume(TokenKind::Identifier, "expected field name");
                ParseNode field = make_node("FieldExpression", field_name.location);
                add_node(field, "base", std::move(expr));
                add_attr(field, "name", field_name.lexeme);
                expr = std::move(field);
                continue;
            }
            if (match(TokenKind::Colon)) {
                Token method = consume(TokenKind::Identifier, "expected method name");
                ParseNode call = make_node("MethodCallExpression", method.location);
                add_node(call, "base", std::move(expr));
                add_attr(call, "method", method.lexeme);
                add_list(call, "arguments", parse_arguments());
                expr = std::move(call);
                continue;
            }
            if (starts_args(current().kind)) {
                ParseNode call = make_node("FunctionCallExpression", current().location);
                add_node(call, "callee", std::move(expr));
                add_list(call, "arguments", parse_arguments());
                expr = std::move(call);
                continue;
            }
            break;
        }
        return expr;
    }

    std::vector<ParseNode> parse_arguments() {
        std::vector<ParseNode> args;
        if (match(TokenKind::LParen)) {
            if (current().kind != TokenKind::RParen) {
                args = parse_expression_list();
            }
            consume(TokenKind::RParen, "expected ')'");
            return args;
        }
        if (current().kind == TokenKind::LBrace) {
            args.push_back(parse_table_constructor());
            return args;
        }
        if (current().kind == TokenKind::String) {
            args.push_back(parse_simple_expression());
            return args;
        }
        error_here("expected function arguments");
        return args;
    }

    static bool starts_args(TokenKind kind) {
        return kind == TokenKind::LParen || kind == TokenKind::LBrace || kind == TokenKind::String;
    }

    static bool is_var_node(const std::string& kind) {
        return kind == "Name" || kind == "IndexExpression" || kind == "FieldExpression";
    }

    static bool is_call_node(const std::string& kind) {
        return kind == "FunctionCallExpression" || kind == "MethodCallExpression";
    }

    static bool is_unary(TokenKind kind) {
        return kind == TokenKind::Minus || kind == TokenKind::KwNot || kind == TokenKind::Hash || kind == TokenKind::Tilde;
    }

    static bool binary_priority(TokenKind kind, int& left, int& right) {
        switch (kind) {
            case TokenKind::KwOr: left = 1; right = 1; return true;
            case TokenKind::KwAnd: left = 2; right = 2; return true;
            case TokenKind::Less:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq:
            case TokenKind::EqEq:
            case TokenKind::NotEq:
                left = 3; right = 3; return true;
            case TokenKind::Pipe: left = 4; right = 4; return true;
            case TokenKind::Tilde: left = 5; right = 5; return true;
            case TokenKind::Amp: left = 6; right = 6; return true;
            case TokenKind::ShiftLeft:
            case TokenKind::ShiftRight:
                left = 7; right = 7; return true;
            case TokenKind::DotDot: left = 8; right = 7; return true;
            case TokenKind::Plus:
            case TokenKind::Minus:
                left = 9; right = 9; return true;
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::SlashSlash:
            case TokenKind::Percent:
                left = 10; right = 10; return true;
            case TokenKind::Caret: left = 12; right = 11; return true;
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
        std::size_t pos = index_ + offset;
        if (pos >= tokens_.size()) {
            return tokens_.back();
        }
        return tokens_[pos];
    }

    Token advance() {
        if (!at_end()) {
            ++index_;
        }
        return tokens_[index_ - 1];
    }

    bool match(TokenKind kind) {
        if (current().kind != kind) {
            return false;
        }
        advance();
        return true;
    }

    Token consume(TokenKind kind, const std::string& message) {
        if (current().kind == kind) {
            return advance();
        }
        error_here(message);
        return Token {kind, "", current().location};
    }

    void error_here(const std::string& message) {
        error_at(current().location, message);
    }

    void error_at(SourceLocation location, const std::string& message) {
        if (!failed_) {
            diagnostics_.push_back({location, message});
        }
        failed_ = true;
    }

    std::vector<Token> tokens_;
    std::size_t index_ {0};
    bool failed_ {false};
    std::vector<Diagnostic> diagnostics_;
};

Parser::Parser(std::vector<Token> tokens) : impl_(std::make_unique<Impl>(std::move(tokens))) {}

Parser::~Parser() = default;
Parser::Parser(Parser&&) noexcept = default;
Parser& Parser::operator=(Parser&&) noexcept = default;

ParseResult Parser::run() {
    return impl_->run();
}

} // namespace suru::front

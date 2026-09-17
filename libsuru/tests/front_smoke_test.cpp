#include "suru/front/dump.hpp"
#include "suru/front/lexer.hpp"
#include "suru/front/parse.hpp"
#include "suru/front/parser.hpp"

#include <string>

#include <gtest/gtest.h>

TEST(FrontSmokeTest, ParsesBasicExpressionTree) {
    auto result = suru::front::parse("return 1 + 2 + 3;");
    ASSERT_TRUE(result.ok());

    const std::string dumped = suru::front::dump(result.tree);
    EXPECT_NE(dumped.find("kind: 'Block'"), std::string::npos);
    EXPECT_NE(dumped.find("kind: 'BinaryExpression'"), std::string::npos);
    EXPECT_NE(dumped.find("value: '3'"), std::string::npos);
}

TEST(FrontSmokeTest, AcceptsUnderscoreLeadingName) {
    auto result = suru::front::parse("_x = 1;");
    EXPECT_TRUE(result.ok());
}

TEST(FrontSmokeTest, SemicolonDoesNotCreateEmptyStatement) {
    auto result = suru::front::parse("break;");
    ASSERT_TRUE(result.ok());

    const std::string dumped = suru::front::dump(result.tree);
    EXPECT_EQ(dumped.find("kind: 'EmptyStatement'"), std::string::npos);
}

TEST(FrontSmokeTest, BracketsAreArrayConstructorsNotLongStrings) {
    auto result = suru::front::parse("return [[abc]]");
    ASSERT_TRUE(result.ok());
    const std::string dumped = suru::front::dump(result.tree);
    EXPECT_NE(dumped.find("kind: 'ArrayConstructor'"), std::string::npos);
}

TEST(FrontSmokeTest, ParsesConfirmedControlAndLiteralSyntax) {
    auto result = suru::front::parse(R"(
outer: while true do
    local a = [1, 2, 3]
    local t = {name = "suru", (a[0]) = true}
    if t.name then continue outer end
    break outer
end
)");
    ASSERT_TRUE(result.ok());
    const std::string dumped = suru::front::dump(result.tree);
    EXPECT_NE(dumped.find("kind: 'ContinueStatement'"), std::string::npos);
    EXPECT_NE(dumped.find("label: 'outer'"), std::string::npos);
    EXPECT_NE(dumped.find("kind: 'TableConstructor'"), std::string::npos);
}

TEST(FrontSmokeTest, RejectsRemovedGotoLabelsAttributesAndImplicitTableFields) {
    EXPECT_FALSE(suru::front::parse("goto done").ok());
    EXPECT_FALSE(suru::front::parse("::done::").ok());
    EXPECT_FALSE(suru::front::parse("local x <close> = 1").ok());
    EXPECT_FALSE(suru::front::parse("return {1, 2}").ok());
    EXPECT_FALSE(suru::front::parse("return {[x] = 1}").ok());
}

TEST(FrontSmokeTest, ExponentRollbackKeepsLocations) {
    {
        const auto tokens = suru::front::tokenize("1e+ x");
        ASSERT_GE(tokens.size(), 5);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].range.begin.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Plus);
        EXPECT_EQ(tokens[2].range.begin.column, 3U);
        EXPECT_EQ(tokens[3].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[3].lexeme, "x");
        EXPECT_EQ(tokens[3].range.begin.column, 5U);
    }

    {
        const auto tokens = suru::front::tokenize("1e x");
        ASSERT_GE(tokens.size(), 4);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].range.begin.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[2].lexeme, "x");
        EXPECT_EQ(tokens[2].range.begin.column, 4U);
    }

    {
        const auto tokens = suru::front::tokenize("1E- y");
        ASSERT_GE(tokens.size(), 5);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].range.begin.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Minus);
        EXPECT_EQ(tokens[2].range.begin.column, 3U);
        EXPECT_EQ(tokens[3].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[3].lexeme, "y");
        EXPECT_EQ(tokens[3].range.begin.column, 5U);
    }
}

TEST(FrontSmokeTest, LeadingDotNumeralIsRejected) {
    const auto tokens = suru::front::tokenize("x = .1");
    ASSERT_GE(tokens.size(), 5);
    EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "x");
    EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Dot);
    EXPECT_EQ(tokens[3].kind, suru::front::TokenKind::Numeral);
    EXPECT_EQ(tokens[3].lexeme, "1");

    auto parsed = suru::front::parse("x = .1");
    EXPECT_FALSE(parsed.ok());
}

TEST(FrontSmokeTest, ReportsUnexpectedEndOfFileLocation) {
    auto result = suru::front::parse("if x then\nreturn 1\n");
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.diagnostics.empty());

    const auto& diag = result.diagnostics.front();
    EXPECT_EQ(diag.message, "unexpected end of file");
    EXPECT_EQ(diag.location.line, 3U);
    EXPECT_EQ(diag.location.column, 1U);
}

TEST(FrontSmokeTest, ParseContextHandlesIncompleteAndRecovery) {
    suru::front::ParseContext repl_context;
    auto incomplete = suru::front::parse("if x then\n", repl_context);
    EXPECT_EQ(incomplete.status, suru::front::ParseStatus::Incomplete);

    suru::front::ParseContext bad_elseif_context;
    auto bad_elseif_result = suru::front::parse("if x then elseif then end\n", bad_elseif_context);
    EXPECT_EQ(bad_elseif_result.status, suru::front::ParseStatus::Error);

    auto recovered = suru::front::parse("return x\nend\n", repl_context);
    EXPECT_EQ(recovered.status, suru::front::ParseStatus::Ok);

    auto invalid = suru::front::parse("local = 1\n", repl_context);
    EXPECT_EQ(invalid.status, suru::front::ParseStatus::Error);
}

TEST(FrontSmokeTest, UsesFnKeywordForFunction) {
    auto ok = suru::front::parse("fn add1(x) return x + 1 end");
    EXPECT_TRUE(ok.ok());

    auto old_keyword = suru::front::parse("function add1(x) return x + 1 end");
    EXPECT_FALSE(old_keyword.ok());
}

TEST(FrontSmokeTest, UsesBangEqualForNotEqual) {
    auto ok = suru::front::parse("return 1 != 2");
    EXPECT_TRUE(ok.ok());

    auto old_operator = suru::front::parse("return 1 ~= 2");
    EXPECT_FALSE(old_operator.ok());
}

TEST(FrontSmokeTest, UsesCaretForXorAndDoubleCaretForPower) {
    auto xor_ok = suru::front::parse("return 1 ^ 2");
    EXPECT_TRUE(xor_ok.ok());

    auto pow_ok = suru::front::parse("return 2 ^^ 3");
    EXPECT_TRUE(pow_ok.ok());

    auto old_xor = suru::front::parse("return 1 ~ 2");
    EXPECT_FALSE(old_xor.ok());
}

namespace {

void expect_range(suru::front::SourceRange range, std::size_t first_line,
    std::size_t first_column, std::size_t last_line, std::size_t last_column) {
    EXPECT_EQ(range.begin.line, first_line);
    EXPECT_EQ(range.begin.column, first_column);
    EXPECT_EQ(range.end.line, last_line);
    EXPECT_EQ(range.end.column, last_column);
}

const suru::front::Expr& returned(const suru::front::ParseResult& result) {
    return *std::get<suru::front::ReturnStmt>(result.tree.root.statements.front()->kind).expressions.front();
}

TEST(FrontRangeTest, TokensTrackActualConsumptionAndMultilineStrings) {
    auto tokens = suru::front::tokenize("abc != 1 \"x\ny\"");
    ASSERT_EQ(tokens.size(), 5U);
    expect_range(tokens[0].range, 1, 1, 1, 4);
    expect_range(tokens[1].range, 1, 5, 1, 7);
    expect_range(tokens[2].range, 1, 8, 1, 9);
    expect_range(tokens[3].range, 1, 10, 2, 3);
    expect_range(tokens[4].range, 2, 3, 2, 3);
    // A token stream without EOF still ends at the final token's end.
    tokens.pop_back();
    const auto end = suru::front::end_location(tokens);
    EXPECT_EQ(end.line, 2U);
    EXPECT_EQ(end.column, 3U);
    auto without_eof = suru::front::tokenize("return 1");
    without_eof.pop_back();
    auto parsed = suru::front::parse_tokens(std::move(without_eof));
    ASSERT_TRUE(parsed.ok());
    expect_range(parsed.tree.root.range, 1, 1, 1, 9);
    auto unterminated = suru::front::tokenize("\"x\ny");
    expect_range(unterminated.front().range, 1, 1, 2, 2);
    EXPECT_EQ(suru::front::parse("return \"x\ny").status, suru::front::ParseStatus::Incomplete);
}

TEST(FrontRangeTest, CoversLiteralsNamesUnaryAndBinaryExpressions) {
    auto parsed = suru::front::parse("return abc + -1");
    ASSERT_TRUE(parsed.ok());
    const auto& expr = returned(parsed);
    expect_range(expr.range, 1, 8, 1, 16);
    const auto& binary = std::get<suru::front::BinaryExpr>(expr.kind);
    expect_range(binary.left->range, 1, 8, 1, 11);
    expect_range(binary.right->range, 1, 14, 1, 16);
    expect_range(std::get<suru::front::UnaryExpr>(binary.right->kind).operand->range, 1, 15, 1, 16);
    expect_range(parsed.tree.root.range, 1, 1, 1, 16);
    expect_range(parsed.tree.root.statements[0]->range, 1, 1, 1, 16);
    const auto yaml = suru::front::dump(parsed.tree);
    EXPECT_NE(yaml.find("begin: { line: 1, column: 8 }"), std::string::npos);
    EXPECT_NE(yaml.find("end: { line: 1, column: 16 }"), std::string::npos);
    EXPECT_EQ(yaml.find("loc:"), std::string::npos);
    EXPECT_EQ(suru::front::parse("return 1 +").status, suru::front::ParseStatus::Incomplete);
    EXPECT_EQ(suru::front::parse("return 1 + )").status, suru::front::ParseStatus::Error);
}

TEST(FrontRangeTest, IncludesParenthesesCallsAndAccessDelimiters) {
    auto parsed = suru::front::parse("return (abc)(1)[0].field:go(2)");
    ASSERT_TRUE(parsed.ok());
    const auto& method_expr = returned(parsed);
    expect_range(method_expr.range, 1, 8, 1, 31);
    const auto& method = std::get<suru::front::MethodCallExpr>(method_expr.kind);
    expect_range(method.receiver->range, 1, 8, 1, 25);
    const auto& field = std::get<suru::front::FieldExpr>(method.receiver->kind);
    expect_range(field.object->range, 1, 8, 1, 19);
    const auto& index = std::get<suru::front::IndexExpr>(field.object->kind);
    expect_range(index.object->range, 1, 8, 1, 16);
    const auto& call = std::get<suru::front::CallExpr>(index.object->kind);
    expect_range(call.callee->range, 1, 8, 1, 13);
}

TEST(FrontRangeTest, ConstructorsAndFieldsIncludeTheirValues) {
    auto parsed = suru::front::parse("return {key = [1, 2]}");
    ASSERT_TRUE(parsed.ok());
    const auto& expr = returned(parsed);
    expect_range(expr.range, 1, 8, 1, 22);
    const auto& table = std::get<suru::front::TableExpr>(expr.kind);
    ASSERT_EQ(table.fields.size(), 1U);
    expect_range(table.fields[0].range, 1, 9, 1, 21);
    expect_range(table.fields[0].key->range, 1, 9, 1, 12);
    expect_range(table.fields[0].value->range, 1, 15, 1, 21);
}

TEST(FrontRangeTest, BlocksFunctionsAndLabelsHaveCompleteRanges) {
    auto parsed = suru::front::parse("local fn f()\n  return fn()\n  end\nend\n");
    ASSERT_TRUE(parsed.ok());
    expect_range(parsed.tree.root.range, 1, 1, 4, 4);
    const auto& local = std::get<suru::front::LocalFunctionStmt>(parsed.tree.root.statements[0]->kind);
    expect_range(local.body->range, 1, 11, 4, 4);
    expect_range(local.body->block->range, 2, 3, 3, 6);
    const auto& ret = std::get<suru::front::ReturnStmt>(local.body->block->statements[0]->kind);
    expect_range(ret.expressions[0]->range, 2, 10, 3, 6);
    const auto& inner = std::get<suru::front::FunctionExpr>(ret.expressions[0]->kind);
    expect_range(inner.body->range, 2, 12, 3, 6);
    expect_range(inner.body->block->range, 3, 3, 3, 3);
    auto empty = suru::front::parse(" \n");
    ASSERT_TRUE(empty.ok());
    expect_range(empty.tree.root.range, 2, 1, 2, 1);
    auto loop = suru::front::parse("outer: while true do\nend");
    ASSERT_TRUE(loop.ok());
    expect_range(loop.tree.root.statements[0]->range, 1, 1, 2, 4);
    expect_range(std::get<suru::front::WhileStmt>(loop.tree.root.statements[0]->kind).block->range, 2, 1, 2, 1);
    auto repeat = suru::front::parse("repeat\n until true");
    ASSERT_TRUE(repeat.ok());
    expect_range(repeat.tree.root.statements[0]->range, 1, 1, 2, 12);
    auto branches = suru::front::parse("if true then\n return 1\nend");
    ASSERT_TRUE(branches.ok());
    const auto& branch = std::get<suru::front::IfStmt>(branches.tree.root.statements[0]->kind).branches[0];
    expect_range(branch.range, 1, 1, 2, 10);
    auto string = suru::front::parse("return \"x\ny\"");
    ASSERT_TRUE(string.ok());
    expect_range(returned(string).range, 1, 8, 2, 3);
}

} // namespace

TEST(FrontSmokeTest, RecoversUnterminatedStringsWithoutLosingSourceOrRanges) {
    for (const char quote : {'\'', '"'}) {
        const std::string first = "return " + std::string(1, quote) + "hello\n";
        suru::front::ParseContext context;
        auto incomplete = suru::front::parse(first, context);
        ASSERT_EQ(incomplete.status, suru::front::ParseStatus::Incomplete);
        EXPECT_EQ(context.pending_source, first);
        ASSERT_EQ(incomplete.diagnostics.size(), 1U);
        EXPECT_EQ(incomplete.diagnostics[0].message, "unterminated string");
        EXPECT_EQ(incomplete.diagnostics[0].location.line, 1U);
        EXPECT_EQ(incomplete.diagnostics[0].location.column, 8U);
        auto complete = suru::front::parse("world" + std::string(1, quote) + "\n", context);
        ASSERT_TRUE(complete.ok());
        EXPECT_TRUE(context.pending_source.empty());
        const auto& value = returned(complete);
        EXPECT_EQ(std::get<suru::front::StringExpr>(value.kind).value, "hello\nworld");
        expect_range(value.range, 1, 8, 2, 7);
        auto following = suru::front::parse("return 1\n", context);
        ASSERT_TRUE(following.ok());
        expect_range(following.tree.root.range, 3, 1, 3, 9);
    }
}

TEST(FrontSmokeTest, AcceptsUnterminatedStringsOnlyInStringPositions) {
    for (const std::string_view source : {
        "return \"abc", "print(\"abc", "print \"abc", "object:method \"abc",
        "return [\"abc", "return {key = \"abc", "return {\"abc"
    }) {
        EXPECT_EQ(suru::front::parse(source).status, suru::front::ParseStatus::Incomplete) << source;
    }
    for (const std::string_view source : {
        "return @", "local \"abc", "local = \"abc", "fn f(\"abc"
    }) {
        EXPECT_EQ(suru::front::parse(source).status, suru::front::ParseStatus::Error) << source;
    }
}

TEST(FrontSmokeTest, UnterminatedEscapeCanBeCompletedByNextFragment) {
    suru::front::ParseContext context;
    const std::string first = "return \"abc\\";
    auto tokens = suru::front::tokenize(first);
    ASSERT_EQ(tokens[1].kind, suru::front::TokenKind::UnterminatedString);
    expect_range(tokens[1].range, 1, 8, 1, 13);
    EXPECT_EQ(suru::front::parse(first, context).status, suru::front::ParseStatus::Incomplete);
    auto recovered = suru::front::parse("\"def\"", context);
    ASSERT_TRUE(recovered.ok());
    EXPECT_EQ(std::get<suru::front::StringExpr>(returned(recovered).kind).value, "abc\"def");
}

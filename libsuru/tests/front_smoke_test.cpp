#include "suru/front/dump.hpp"
#include "suru/front/lexer.hpp"
#include "suru/front/parse.hpp"

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

TEST(FrontSmokeTest, RejectsLongStringSyntax) {
    auto result = suru::front::parse("return [[abc]]");
    EXPECT_FALSE(result.ok());
}

TEST(FrontSmokeTest, ExponentRollbackKeepsLocations) {
    {
        const auto tokens = suru::front::tokenize("1e+ x");
        ASSERT_GE(tokens.size(), 5);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].location.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Plus);
        EXPECT_EQ(tokens[2].location.column, 3U);
        EXPECT_EQ(tokens[3].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[3].lexeme, "x");
        EXPECT_EQ(tokens[3].location.column, 5U);
    }

    {
        const auto tokens = suru::front::tokenize("1e x");
        ASSERT_GE(tokens.size(), 4);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].location.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[2].lexeme, "x");
        EXPECT_EQ(tokens[2].location.column, 4U);
    }

    {
        const auto tokens = suru::front::tokenize("1E- y");
        ASSERT_GE(tokens.size(), 5);
        EXPECT_EQ(tokens[0].kind, suru::front::TokenKind::Numeral);
        EXPECT_EQ(tokens[0].lexeme, "1");
        EXPECT_EQ(tokens[1].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[1].location.column, 2U);
        EXPECT_EQ(tokens[2].kind, suru::front::TokenKind::Minus);
        EXPECT_EQ(tokens[2].location.column, 3U);
        EXPECT_EQ(tokens[3].kind, suru::front::TokenKind::Identifier);
        EXPECT_EQ(tokens[3].lexeme, "y");
        EXPECT_EQ(tokens[3].location.column, 5U);
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


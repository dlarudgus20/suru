#include "suru/vm/vm.hpp"

#include <gtest/gtest.h>

TEST(VmSmokeTest, CreatesVmAndGlobalTable) {
    suru::vm::VM vm;
    EXPECT_NE(vm.globals(), nullptr);
}

TEST(VmSmokeTest, InternsStrings) {
    suru::vm::VM vm;

    suru::vm::String* s1 = vm.load_string("alpha");
    suru::vm::String* s2 = vm.load_string("alpha");
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(s1, s2);
}

TEST(VmSmokeTest, SupportsTableCrud) {
    suru::vm::VM vm;

    suru::vm::Table* t = vm.load_table();
    ASSERT_NE(t, nullptr);

    suru::vm::String* key_str = vm.load_string("k");
    ASSERT_NE(key_str, nullptr);

    suru::vm::Value key = suru::vm::Value::string(key_str);
    suru::vm::Value value = suru::vm::Value::number(42.0);
    EXPECT_TRUE(t->set(key, value));
    EXPECT_TRUE(t->has(key));

    suru::vm::Value out = suru::vm::Value::nil();
    EXPECT_TRUE(t->get(key, &out));
    EXPECT_TRUE(suru::vm::value_equals(out, value));

    EXPECT_TRUE(t->erase(key));
    EXPECT_FALSE(t->has(key));
}

TEST(VmSmokeTest, InjectsAndReadsGlobalValuesThroughGlobalTable) {
    suru::vm::VM vm;

    suru::vm::Table* g = vm.globals();
    ASSERT_NE(g, nullptr);

    suru::vm::String* key_str = vm.load_string("answer");
    ASSERT_NE(key_str, nullptr);

    suru::vm::Value key = suru::vm::Value::string(key_str);
    suru::vm::Value value = suru::vm::Value::number(42.0);
    EXPECT_TRUE(g->set(key, value));

    suru::vm::Value out = suru::vm::Value::nil();
    EXPECT_TRUE(g->get(key, &out));
    EXPECT_TRUE(suru::vm::value_equals(out, value));
}

TEST(VmSmokeTest, UsesConfiguredKeyEqualityRules) {
    suru::vm::VM vm;

    suru::vm::Table* t = vm.load_table();
    ASSERT_NE(t, nullptr);

    suru::vm::Value key = suru::vm::Value::number(1.0);
    EXPECT_TRUE(t->set(key, suru::vm::Value::boolean(true)));

    suru::vm::Value out = suru::vm::Value::nil();
    EXPECT_TRUE(t->get(key, &out));
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Boolean);
    EXPECT_TRUE(out.bool_);

    suru::vm::Table* key_table_1 = vm.load_table();
    suru::vm::Table* key_table_2 = vm.load_table();
    ASSERT_NE(key_table_1, nullptr);
    ASSERT_NE(key_table_2, nullptr);
    EXPECT_TRUE(t->set(suru::vm::Value::table(key_table_1), suru::vm::Value::number(7.0)));
    EXPECT_FALSE(t->get(suru::vm::Value::table(key_table_2), &out));
}

TEST(VmSmokeTest, CreatesClosureWithInitializedSlots) {
    suru::vm::VM vm;

    suru::vm::CFunction func = [](auto vm, auto self) {};

    suru::vm::Closure* closure = vm.load_closure_c(func, 3);
    ASSERT_NE(closure, nullptr);
    EXPECT_EQ(closure->cfunc, func);
    EXPECT_EQ(closure->len, 3U);

    EXPECT_EQ(closure->at(0).kind, suru::vm::ValueKind::Nil);
    EXPECT_EQ(closure->at(1).kind, suru::vm::ValueKind::Nil);
    EXPECT_EQ(closure->at(2).kind, suru::vm::ValueKind::Nil);

    closure->at(1) = suru::vm::Value::number(9.0);
    EXPECT_EQ(closure->at(1).kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(closure->at(1).number_, 9.0);
}

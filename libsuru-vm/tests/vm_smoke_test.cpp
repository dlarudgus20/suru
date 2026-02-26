#include "suru/vm/vm.hpp"
#include "suru/vm/opcode.hpp"
#include "suru/lib/lib.hpp"

#include <gtest/gtest.h>

namespace {

void append_uleb(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (true) {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fU);
        value >>= 7U;
        if (value != 0) {
            byte |= 0x80U;
        }
        out.push_back(byte);
        if (value == 0) {
            break;
        }
    }
}

} // namespace

TEST(VmSmokeTest, CreatesVmAndGlobalTable) {
    suru::vm::VM vm;
    EXPECT_NE(vm.globals(), nullptr);
}

TEST(VmSmokeTest, LoadsStdPrintIntoGlobals) {
    suru::vm::VM vm;
    suru::lib::load_libs(vm);

    suru::vm::String* print_name = vm.make_string("print");
    ASSERT_NE(print_name, nullptr);

    suru::vm::Value print_value = suru::vm::Value::nil();
    EXPECT_TRUE(vm.globals()->get(suru::vm::Value::string(print_name), &print_value));
    EXPECT_EQ(print_value.kind, suru::vm::ValueKind::Closure);
    ASSERT_NE(print_value.closure_, nullptr);
    EXPECT_EQ(print_value.closure_->code, nullptr);
    EXPECT_NE(print_value.closure_->cfunc, nullptr);
}

TEST(VmSmokeTest, LoadsStdModulesAndFunctions) {
    suru::vm::VM vm;
    suru::lib::load_libs(vm);

    auto expect_global_closure = [&](std::string_view name) {
        suru::vm::Value out = suru::vm::Value::nil();
        ASSERT_TRUE(vm.globals()->get(suru::vm::Value::string(vm.make_string(name)), &out));
        ASSERT_EQ(out.kind, suru::vm::ValueKind::Closure);
        ASSERT_NE(out.closure_, nullptr);
        ASSERT_NE(out.closure_->cfunc, nullptr);
    };

    auto expect_module_closure = [&](std::string_view mod_name, std::string_view fn_name) {
        suru::vm::Value mod = suru::vm::Value::nil();
        ASSERT_TRUE(vm.globals()->get(suru::vm::Value::string(vm.make_string(mod_name)), &mod));
        ASSERT_EQ(mod.kind, suru::vm::ValueKind::Table);
        ASSERT_NE(mod.table_, nullptr);

        suru::vm::Value fn = suru::vm::Value::nil();
        ASSERT_TRUE(mod.table_->get(suru::vm::Value::string(vm.make_string(fn_name)), &fn));
        ASSERT_EQ(fn.kind, suru::vm::ValueKind::Closure);
        ASSERT_NE(fn.closure_, nullptr);
        ASSERT_NE(fn.closure_->cfunc, nullptr);
    };

    expect_global_closure("print");
    expect_global_closure("read_line");
    expect_global_closure("to_number");
    expect_global_closure("to_string");
    expect_global_closure("type");

    expect_module_closure("str", "trim");
    expect_module_closure("str", "split");
    expect_module_closure("table", "keys");
    expect_module_closure("table", "push");
    expect_module_closure("table", "pop");
    expect_module_closure("math", "abs");
    expect_module_closure("math", "floor");
    expect_module_closure("math", "ceil");
}

TEST(VmSmokeTest, InternsStrings) {
    suru::vm::VM vm;

    suru::vm::String* s1 = vm.make_string("alpha");
    suru::vm::String* s2 = vm.make_string("alpha");
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);
    EXPECT_EQ(s1, s2);
}

TEST(VmSmokeTest, SupportsTableCrud) {
    suru::vm::VM vm;

    suru::vm::Table* t = vm.make_table();
    ASSERT_NE(t, nullptr);

    suru::vm::String* key_str = vm.make_string("k");
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

    suru::vm::String* key_str = vm.make_string("answer");
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

    suru::vm::Table* t = vm.make_table();
    ASSERT_NE(t, nullptr);

    suru::vm::Value key = suru::vm::Value::number(1.0);
    EXPECT_TRUE(t->set(key, suru::vm::Value::boolean(true)));

    suru::vm::Value out = suru::vm::Value::nil();
    EXPECT_TRUE(t->get(key, &out));
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Boolean);
    EXPECT_TRUE(out.bool_);

    suru::vm::Table* key_table_1 = vm.make_table();
    suru::vm::Table* key_table_2 = vm.make_table();
    ASSERT_NE(key_table_1, nullptr);
    ASSERT_NE(key_table_2, nullptr);
    EXPECT_TRUE(t->set(suru::vm::Value::table(key_table_1), suru::vm::Value::number(7.0)));
    EXPECT_FALSE(t->get(suru::vm::Value::table(key_table_2), &out));
}

TEST(VmSmokeTest, CreatesClosureWithInitializedSlots) {
    suru::vm::VM vm;

    suru::vm::CFunction func = [](auto vm, auto self) {};

    suru::vm::Closure* closure = vm.make_closure_c(func, 3);
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

TEST(VmSmokeTest, ExecutesBytecodeAndWritesGlobal) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);

    suru::vm::String* key_name = vm.make_string("answer");
    ASSERT_NE(key_name, nullptr);

    cu->constants_.push_back(suru::vm::Value::number(42.0));
    cu->constants_.push_back(suru::vm::Value::string(key_name));

    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 0);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::SetGlobal));
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 0);

    cu->chunks_.push_back(suru::vm::Chunk {"main", 0, cu->opcodes_.size(), 0, 4, 0});

    suru::vm::Closure* closure = vm.make_closure(cu, 0, 0);
    ASSERT_NE(closure, nullptr);

    vm.push_value(suru::vm::Value::closure(closure));
    EXPECT_NO_THROW(vm.call(0, 0));

    suru::vm::Value out = suru::vm::Value::nil();
    EXPECT_TRUE(vm.globals()->get(suru::vm::Value::string(key_name), &out));
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 42.0);
}

TEST(VmSmokeTest, CallsChunkClosureAndReturnsValue) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(7.0));

    const std::size_t main_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Closure));
    append_uleb(cu->opcodes_, 1); // foo chunk index
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Call));
    append_uleb(cu->opcodes_, 0);
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 1);
    const std::size_t main_end = cu->opcodes_.size();

    const std::size_t foo_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 0);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 1);
    const std::size_t foo_end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 4, 0});
    cu->chunks_.push_back(suru::vm::Chunk {"foo", foo_begin, foo_end, 0, 2, 0});

    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 7.0);
}

TEST(VmSmokeTest, PadsMissingArgsWithNilByArity) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(7.0));

    const std::size_t main_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Closure));
    append_uleb(cu->opcodes_, 1); // foo chunk index
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 0);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Call));
    append_uleb(cu->opcodes_, 1);
    append_uleb(cu->opcodes_, 2);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 2);
    const std::size_t main_end = cu->opcodes_.size();

    const std::size_t foo_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::GetLocal));
    append_uleb(cu->opcodes_, 0);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::GetLocal));
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 2);
    const std::size_t foo_end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 4, 0});
    cu->chunks_.push_back(suru::vm::Chunk {"foo", foo_begin, foo_end, 2, 2, 0});

    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 2));

    ASSERT_GE(vm.stack_top(), 2U);
    suru::vm::Value second = vm.pop_value();
    suru::vm::Value first = vm.pop_value();
    EXPECT_EQ(first.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(first.number_, 7.0);
    EXPECT_EQ(second.kind, suru::vm::ValueKind::Nil);
}

TEST(VmSmokeTest, TruncatesExtraArgsByArity) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(10.0));
    cu->constants_.push_back(suru::vm::Value::number(20.0));
    cu->constants_.push_back(suru::vm::Value::number(30.0));

    const std::size_t main_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Closure));
    append_uleb(cu->opcodes_, 1); // foo chunk index
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 0);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Const));
    append_uleb(cu->opcodes_, 2);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Call));
    append_uleb(cu->opcodes_, 3);
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 1);
    const std::size_t main_end = cu->opcodes_.size();

    const std::size_t foo_begin = cu->opcodes_.size();
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::GetLocal));
    append_uleb(cu->opcodes_, 1);
    cu->opcodes_.push_back(static_cast<std::uint8_t>(suru::vm::Op::Return));
    append_uleb(cu->opcodes_, 1);
    const std::size_t foo_end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 6, 0});
    cu->chunks_.push_back(suru::vm::Chunk {"foo", foo_begin, foo_end, 2, 2, 0});

    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 20.0);
}


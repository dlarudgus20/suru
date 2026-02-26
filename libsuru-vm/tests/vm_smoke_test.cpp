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

void emit1(std::vector<std::uint8_t>& out, suru::vm::Op op, std::size_t a) {
    out.push_back(static_cast<std::uint8_t>(op));
    append_uleb(out, a);
}

void emit2(std::vector<std::uint8_t>& out, suru::vm::Op op, std::size_t a, std::size_t b) {
    out.push_back(static_cast<std::uint8_t>(op));
    append_uleb(out, a);
    append_uleb(out, b);
}

void emit3(std::vector<std::uint8_t>& out, suru::vm::Op op, std::size_t a, std::size_t b, std::size_t c) {
    out.push_back(static_cast<std::uint8_t>(op));
    append_uleb(out, a);
    append_uleb(out, b);
    append_uleb(out, c);
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
}

TEST(VmSmokeTest, ExecutesRegisterBytecodeAndWritesGlobal) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);

    suru::vm::String* key_name = vm.make_string("answer");
    ASSERT_NE(key_name, nullptr);

    cu->constants_.push_back(suru::vm::Value::number(42.0));
    cu->constants_.push_back(suru::vm::Value::string(key_name));

    emit2(cu->opcodes_, suru::vm::Op::LoadK, 0, 0);
    emit2(cu->opcodes_, suru::vm::Op::SetGlobal, 1, 0);
    emit2(cu->opcodes_, suru::vm::Op::Return, 0, 0);

    cu->chunks_.push_back(suru::vm::Chunk {"main", 0, cu->opcodes_.size(), 0, 2, 0});

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
    emit2(cu->opcodes_, suru::vm::Op::Closure, 0, 1);
    emit3(cu->opcodes_, suru::vm::Op::Call, 0, 0, 1);
    emit2(cu->opcodes_, suru::vm::Op::Return, 0, 1);
    const std::size_t main_end = cu->opcodes_.size();

    const std::size_t foo_begin = cu->opcodes_.size();
    emit2(cu->opcodes_, suru::vm::Op::LoadK, 0, 0);
    emit2(cu->opcodes_, suru::vm::Op::Return, 0, 1);
    const std::size_t foo_end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 2, 0});
    cu->chunks_.push_back(suru::vm::Chunk {"foo", foo_begin, foo_end, 0, 1, 0});

    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 7.0);
}

TEST(VmSmokeTest, SupportsUpvalueCaptureAndMutation) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(10.0));
    cu->constants_.push_back(suru::vm::Value::number(1.0));

    const std::size_t main_begin = cu->opcodes_.size();
    emit2(cu->opcodes_, suru::vm::Op::LoadK, 1, 0);
    emit2(cu->opcodes_, suru::vm::Op::Closure, 0, 1);
    emit3(cu->opcodes_, suru::vm::Op::Call, 0, 0, 1);
    emit2(cu->opcodes_, suru::vm::Op::Return, 0, 1);
    const std::size_t main_end = cu->opcodes_.size();

    const std::size_t inc_begin = cu->opcodes_.size();
    emit2(cu->opcodes_, suru::vm::Op::GetUpvalue, 0, 0);
    emit2(cu->opcodes_, suru::vm::Op::LoadK, 1, 1);
    emit3(cu->opcodes_, suru::vm::Op::Add, 0, 0, 1);
    emit2(cu->opcodes_, suru::vm::Op::SetUpvalue, 0, 0);
    emit2(cu->opcodes_, suru::vm::Op::GetUpvalue, 0, 0);
    emit2(cu->opcodes_, suru::vm::Op::Return, 0, 1);
    const std::size_t inc_end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 3, 0});
    cu->chunks_.push_back(suru::vm::Chunk {"inc", inc_begin, inc_end, 0, 2, 1});

    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 11.0);
}

TEST(VmSmokeTest, EvaluatesBooleanAndOrOpcodes) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);

    const std::size_t begin = cu->opcodes_.size();
    emit1(cu->opcodes_, suru::vm::Op::LoadTrue, 0);
    emit1(cu->opcodes_, suru::vm::Op::LoadFalse, 1);
    emit3(cu->opcodes_, suru::vm::Op::And, 2, 0, 1);
    emit3(cu->opcodes_, suru::vm::Op::Or, 3, 0, 1);
    emit2(cu->opcodes_, suru::vm::Op::Return, 2, 2);
    const std::size_t end = cu->opcodes_.size();

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 4, 0});
    suru::vm::Closure* entry = vm.make_closure(cu, 0, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 2));

    ASSERT_GE(vm.stack_top(), 2U);
    const suru::vm::Value second = vm.pop_value();
    const suru::vm::Value first = vm.pop_value();
    ASSERT_EQ(first.kind, suru::vm::ValueKind::Boolean);
    ASSERT_EQ(second.kind, suru::vm::ValueKind::Boolean);
    EXPECT_FALSE(first.bool_);
    EXPECT_TRUE(second.bool_);
}

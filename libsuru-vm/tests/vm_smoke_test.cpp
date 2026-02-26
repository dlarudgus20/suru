#include "suru/vm/vm.hpp"
#include "suru/vm/opcode.hpp"
#include "suru/lib/lib.hpp"

#include <limits>

#include <gtest/gtest.h>

namespace {

std::uint32_t to_u32(std::size_t v) {
    if (v > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("value out of uint32 range");
    }
    return static_cast<std::uint32_t>(v);
}

std::uint32_t pack_abc(suru::vm::Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return (static_cast<std::uint32_t>(op) << 26U) | ((a & 0xFFU) << 17U) | ((b & 0xFFU) << 9U) | (c & 0x1FFU);
}

std::uint32_t pack_abc_i(suru::vm::Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool i) {
    return (static_cast<std::uint32_t>(op) << 26U)
        | ((i ? 1U : 0U) << 25U)
        | ((a & 0xFFU) << 17U)
        | ((b & 0xFFU) << 9U)
        | (c & 0x1FFU);
}

std::uint32_t pack_abx(suru::vm::Op op, std::uint32_t a, std::uint32_t bx, bool i = false) {
    return (static_cast<std::uint32_t>(op) << 26U)
        | ((i ? 1U : 0U) << 25U)
        | ((a & 0xFFU) << 17U)
        | (bx & 0x1FFFFU);
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

    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::SetGlobalK, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 0));

    cu->chunks_.push_back(suru::vm::Chunk {"main", 0, to_u32(cu->code_.size()), 0, 2, {}});

    suru::vm::Closure* closure = vm.make_closure(cu, 0);
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

    const std::uint32_t main_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::Closure, 0, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t main_end = to_u32(cu->code_.size());

    const std::uint32_t foo_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t foo_end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 2, {}});
    cu->chunks_.push_back(suru::vm::Chunk {"foo", foo_begin, foo_end, 0, 1, {}});

    suru::vm::Closure* entry = vm.make_closure(cu, 0);
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

    const std::uint32_t main_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Closure, 0, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t main_end = to_u32(cu->code_.size());

    const std::uint32_t inc_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::GetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Add, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::SetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::GetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t inc_end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 3, {}});
    cu->chunks_.push_back(
        suru::vm::Chunk {
            "inc",
            inc_begin,
            inc_end,
            0,
            2,
            {{suru::vm::UpvalueSource::Local, 1}},
        }
    );

    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 11.0);
}

TEST(VmSmokeTest, KeepsUpvalueAliveAfterOuterReturns) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(40.0));
    cu->constants_.push_back(suru::vm::Value::number(1.0));

    const std::uint32_t main_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Closure, 0, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Load, 3, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Load, 0, 3));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Load, 0, 3));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t main_end = to_u32(cu->code_.size());

    const std::uint32_t make_inc_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::Closure, 0, 2));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t make_inc_end = to_u32(cu->code_.size());

    const std::uint32_t inc_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::GetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Add, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::SetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::GetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t inc_end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 4, {}});
    cu->chunks_.push_back(
        suru::vm::Chunk {
            "make_inc",
            make_inc_begin,
            make_inc_end,
            0,
            2,
            {{suru::vm::UpvalueSource::Local, 2}},
        }
    );
    cu->chunks_.push_back(
        suru::vm::Chunk {
            "inc",
            inc_begin,
            inc_end,
            0,
            2,
            {{suru::vm::UpvalueSource::Upvalue, 0}},
        }
    );

    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    EXPECT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 42.0);
}

TEST(VmSmokeTest, EvaluatesBooleanAndOrOpcodes) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadTrue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadFalse, 1, 0));
    cu->code_.push_back(pack_abc(suru::vm::Op::And, 2, 0, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Or, 3, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 2, 2));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 4, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
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

TEST(VmSmokeTest, ConcatsStringNumberBoolean) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    suru::vm::String* hello = vm.make_string("hello");
    ASSERT_NE(hello, nullptr);
    cu->constants_.push_back(suru::vm::Value::string(hello));
    cu->constants_.push_back(suru::vm::Value::number(1.0));
    cu->constants_.push_back(suru::vm::Value::boolean(true));

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Concat, 2, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 3, 2));
    cu->code_.push_back(pack_abc(suru::vm::Op::Concat, 4, 2, 3));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 4, 1));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 5, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    ASSERT_EQ(out.kind, suru::vm::ValueKind::String);
    ASSERT_NE(out.string_, nullptr);
    EXPECT_EQ(out.string_->view(), "hello1true");
}

TEST(VmSmokeTest, LenOnString) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    suru::vm::String* hello = vm.make_string("hello");
    ASSERT_NE(hello, nullptr);
    cu->constants_.push_back(suru::vm::Value::string(hello));

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Len, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 1, 1));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 2, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    ASSERT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 5.0);
}

TEST(VmSmokeTest, ArrayCreateReadWriteAndNegativeIndex) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(3.0));   // len
    cu->constants_.push_back(suru::vm::Value::number(0.0));   // idx0
    cu->constants_.push_back(suru::vm::Value::number(2.0));   // idx2
    cu->constants_.push_back(suru::vm::Value::number(-1.0));  // idx -1
    cu->constants_.push_back(suru::vm::Value::number(10.0));  // v10
    cu->constants_.push_back(suru::vm::Value::number(20.0));  // v20

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::NewArray, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 3, 4));
    cu->code_.push_back(pack_abc(suru::vm::Op::SetArray, 1, 2, 3));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 2));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 3, 5));
    cu->code_.push_back(pack_abc(suru::vm::Op::SetArray, 1, 2, 3));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 3));
    cu->code_.push_back(pack_abc(suru::vm::Op::GetArray, 4, 1, 2));
    cu->code_.push_back(pack_abx(suru::vm::Op::Len, 5, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 4, 2));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 6, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 2));

    ASSERT_GE(vm.stack_top(), 2U);
    const suru::vm::Value len_out = vm.pop_value();
    const suru::vm::Value value_out = vm.pop_value();
    ASSERT_EQ(value_out.kind, suru::vm::ValueKind::Number);
    ASSERT_EQ(len_out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value_out.number_, 20.0);
    EXPECT_EQ(len_out.number_, 3.0);
}

TEST(VmSmokeTest, ArrayIndexOutOfBoundsThrows) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(2.0));   // len
    cu->constants_.push_back(suru::vm::Value::number(-3.0));  // oob negative

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::NewArray, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::GetArray, 3, 1, 2));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 0));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 4, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_THROW(vm.call(0, 0), suru::vm::TypeError);
}

TEST(VmSmokeTest, ConcatAndLenRejectInvalidTypes) {
    {
        suru::vm::VM vm;
        suru::vm::CodeUnit* concat_cu = vm.make_code_unit();
        ASSERT_NE(concat_cu, nullptr);
        concat_cu->constants_.push_back(suru::vm::Value::table(vm.make_table()));
        concat_cu->constants_.push_back(suru::vm::Value::number(1.0));

        const std::uint32_t concat_begin = to_u32(concat_cu->code_.size());
        concat_cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
        concat_cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 1));
        concat_cu->code_.push_back(pack_abc(suru::vm::Op::Concat, 2, 0, 1));
        concat_cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 0));
        const std::uint32_t concat_end = to_u32(concat_cu->code_.size());
        concat_cu->chunks_.push_back(suru::vm::Chunk {"main", concat_begin, concat_end, 0, 3, {}});

        suru::vm::Closure* concat_entry = vm.make_closure(concat_cu, 0);
        ASSERT_NE(concat_entry, nullptr);
        vm.push_value(suru::vm::Value::closure(concat_entry));
        EXPECT_THROW(vm.call(0, 0), suru::vm::TypeError);
    }

    {
        suru::vm::VM vm;
        suru::vm::CodeUnit* len_cu = vm.make_code_unit();
        ASSERT_NE(len_cu, nullptr);
        len_cu->constants_.push_back(suru::vm::Value::number(3.0));

        const std::uint32_t len_begin = to_u32(len_cu->code_.size());
        len_cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
        len_cu->code_.push_back(pack_abx(suru::vm::Op::Len, 1, 0));
        len_cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 0));
        const std::uint32_t len_end = to_u32(len_cu->code_.size());
        len_cu->chunks_.push_back(suru::vm::Chunk {"main", len_begin, len_end, 0, 2, {}});

        suru::vm::Closure* len_entry = vm.make_closure(len_cu, 0);
        ASSERT_NE(len_entry, nullptr);
        vm.push_value(suru::vm::Value::closure(len_entry));
        EXPECT_THROW(vm.call(0, 0), suru::vm::TypeError);
    }
}

TEST(VmSmokeTest, SupportsImmediateOperands) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(7.0));   // R0
    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abc_i(suru::vm::Op::Add, 1, 0, 2, true));        // 7 + 2 = 9
    cu->code_.push_back(pack_abc_i(suru::vm::Op::Eq, 2, 1, 9, true));         // 9 == 9
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 1, 2));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 3, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 2));

    ASSERT_GE(vm.stack_top(), 2U);
    const suru::vm::Value eq_out = vm.pop_value();
    const suru::vm::Value add_out = vm.pop_value();

    ASSERT_EQ(add_out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(add_out.number_, 9.0);
    ASSERT_EQ(eq_out.kind, suru::vm::ValueKind::Boolean);
    EXPECT_TRUE(eq_out.bool_);
}

TEST(VmSmokeTest, SupportsImmediateArrayOperands) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(7.0));   // value
    cu->constants_.push_back(suru::vm::Value::number(-1.0));  // reg index

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::NewArray, 1, 4, true));        // len immediate 4
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 2, 1));                 // -1
    cu->code_.push_back(pack_abc(suru::vm::Op::SetArray, 1, 2, 0));           // arr[-1] = 7
    cu->code_.push_back(pack_abc_i(suru::vm::Op::GetArray, 3, 1, 0x1FFU, true)); // arr[#-1]
    cu->code_.push_back(pack_abx(suru::vm::Op::Len, 4, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 3, 2));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 5, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 2));

    ASSERT_GE(vm.stack_top(), 2U);
    const suru::vm::Value len_out = vm.pop_value();
    const suru::vm::Value get_out = vm.pop_value();
    ASSERT_EQ(get_out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(get_out.number_, 7.0);
    ASSERT_EQ(len_out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(len_out.number_, 4.0);
}

TEST(VmSmokeTest, SupportsSetGlobalKImmediate) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    suru::vm::String* key = vm.make_string("imm_global");
    ASSERT_NE(key, nullptr);
    cu->constants_.push_back(suru::vm::Value::string(key));

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::SetGlobalK, 0, 123, true));
    cu->code_.push_back(pack_abx(suru::vm::Op::GetGlobalK, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 1, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    ASSERT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 123.0);
}

TEST(VmSmokeTest, SupportsSetUpvalueImmediate) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    cu->constants_.push_back(suru::vm::Value::number(1.0));

    const std::uint32_t main_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 1, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Closure, 0, 1));
    cu->code_.push_back(pack_abc(suru::vm::Op::Call, 0, 0, 1));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t main_end = to_u32(cu->code_.size());

    const std::uint32_t inner_begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::SetUpvalue, 0, 101, true));
    cu->code_.push_back(pack_abx(suru::vm::Op::GetUpvalue, 0, 0));
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 0, 1));
    const std::uint32_t inner_end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", main_begin, main_end, 0, 2, {}});
    cu->chunks_.push_back(
        suru::vm::Chunk {
            "inner",
            inner_begin,
            inner_end,
            0,
            1,
            {{suru::vm::UpvalueSource::Local, 1}},
        }
    );

    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 1));

    ASSERT_GE(vm.stack_top(), 1U);
    const suru::vm::Value out = vm.pop_value();
    ASSERT_EQ(out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(out.number_, 101.0);
}

TEST(VmSmokeTest, SupportsRegisterGlobalAndArrayI) {
    suru::vm::VM vm;

    suru::vm::CodeUnit* cu = vm.make_code_unit();
    ASSERT_NE(cu, nullptr);
    suru::vm::String* key = vm.make_string("x");
    ASSERT_NE(key, nullptr);
    cu->constants_.push_back(suru::vm::Value::string(key));

    const std::uint32_t begin = to_u32(cu->code_.size());
    cu->code_.push_back(pack_abx(suru::vm::Op::LoadK, 0, 0));                    // R0 = "x"
    cu->code_.push_back(pack_abx(suru::vm::Op::SetGlobal, 0, 123, true));        // G[R0] = #123
    cu->code_.push_back(pack_abx(suru::vm::Op::GetGlobal, 0, 1));                // R1 = G[R0]
    cu->code_.push_back(pack_abx(suru::vm::Op::NewArray, 2, 3, true));           // R2 = newarray(3)
    cu->code_.push_back(pack_abc_i(suru::vm::Op::SetArrayI, 2, 0xFFU, 77, true)); // R2[I[-1]] = #77
    cu->code_.push_back(pack_abc(suru::vm::Op::GetArrayI, 3, 0xFFU, 2));         // R3 = R2[I[-1]]
    cu->code_.push_back(pack_abx(suru::vm::Op::Len, 4, 2));                       // R4 = len(R2)
    cu->code_.push_back(pack_abx(suru::vm::Op::Return, 1, 4));
    const std::uint32_t end = to_u32(cu->code_.size());

    cu->chunks_.push_back(suru::vm::Chunk {"main", begin, end, 0, 5, {}});
    suru::vm::Closure* entry = vm.make_closure(cu, 0);
    ASSERT_NE(entry, nullptr);
    vm.push_value(suru::vm::Value::closure(entry));
    EXPECT_NO_THROW(vm.call(0, 4));

    ASSERT_GE(vm.stack_top(), 4U);
    const suru::vm::Value len_out = vm.pop_value();
    const suru::vm::Value arr_out = vm.pop_value();
    const suru::vm::Value array_out = vm.pop_value();
    const suru::vm::Value global_out = vm.pop_value();
    ASSERT_EQ(global_out.kind, suru::vm::ValueKind::Number);
    ASSERT_EQ(array_out.kind, suru::vm::ValueKind::Array);
    ASSERT_EQ(arr_out.kind, suru::vm::ValueKind::Number);
    ASSERT_EQ(len_out.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(global_out.number_, 123.0);
    EXPECT_EQ(arr_out.number_, 77.0);
    EXPECT_EQ(len_out.number_, 3.0);
}

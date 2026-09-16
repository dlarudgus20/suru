#include "suru/vm/vm.hpp"
#include "suru/vm/opcode.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <initializer_list>
#include <vector>

namespace {
using namespace suru::vm;

std::uint32_t abx(Op op, std::uint32_t a, std::uint32_t bx = 0, bool v = false) {
    return (std::uint32_t(op) << 26U) | (std::uint32_t(v) << 25U) | (a << 17U) | bx;
}
std::uint32_t abc(Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool v = false) {
    return abx(op, a, (b << 9U) | c, v);
}

class VarargTest : public testing::Test {
protected:
    VM vm;
    CodeUnit* cu = vm.make_code_unit();

    void chunk(std::uint8_t arity, std::uint8_t slots, std::initializer_list<std::uint32_t> code,
               std::vector<UpvalueInfo> upvalues = {}) {
        const auto begin = static_cast<std::uint32_t>(cu->code_.size());
        cu->code_.insert(cu->code_.end(), code);
        cu->chunks_.push_back(Chunk {"test", begin, static_cast<std::uint32_t>(cu->code_.size()),
                                    arity, slots, std::move(upvalues)});
    }
    void invoke(std::initializer_list<double> args, std::uint16_t retc = VM::multret) {
        vm.push_value(Value::closure(vm.make_closure(cu, 0)));
        for (double x : args) vm.push_value(Value::number(x));
        vm.call(static_cast<std::uint32_t>(args.size()), retc);
    }
    void numbers(std::initializer_list<double> expected) {
        ASSERT_EQ(vm.stack_top(), expected.size());
        std::uint32_t i = 0;
        for (double x : expected) {
            const auto value = vm.getlocal(i++);
            ASSERT_EQ(value.kind, ValueKind::Number);
            EXPECT_EQ(value.number_, x);
        }
    }
};

TEST_F(VarargTest, FixedArityStillPadsAndDiscards) {
    chunk(2, 2, {abx(Op::Return, 0, 2)});
    invoke({10});
    ASSERT_EQ(vm.stack_top(), 2U);
    EXPECT_EQ(vm.getlocal(0).number_, 10);
    EXPECT_EQ(vm.getlocal(1).kind, ValueKind::Nil);
    (void)vm.pop_value(); (void)vm.pop_value();
    invoke({10, 20, 30});
    numbers({10, 20});
}

TEST_F(VarargTest, PrepPadsMissingFixedAndVargPadsMissingExtras) {
    chunk(255, 4, {abx(Op::VargPrep, 2), abx(Op::Varg, 2, 2), abx(Op::Return, 0, 4)});
    invoke({10});
    ASSERT_EQ(vm.stack_top(), 4U);
    EXPECT_EQ(vm.getlocal(0).number_, 10);
    for (unsigned i = 1; i < 4; ++i) EXPECT_EQ(vm.getlocal(i).kind, ValueKind::Nil);
}

TEST_F(VarargTest, OpenVargHasNoPadding) {
    chunk(255, 3, {abx(Op::VargPrep, 1), abx(Op::Varg, 1, 0, true), abx(Op::Return, 0, 0, true)});
    invoke({10, 20, 30});
    numbers({10, 20, 30});
}

TEST_F(VarargTest, ReturnIncomingArgsWithoutPrep) {
    chunk(255, 1, {abx(Op::Return, 0, 0, true)});
    invoke({10, 20, 30});
    numbers({10, 20, 30});
}

TEST_F(VarargTest, ZeroArgsAndZeroSlots) {
    chunk(255, 0, {abx(Op::VargPrep, 0), abx(Op::VargPrep, 0),
                   abx(Op::Varg, 0, 0, true), abx(Op::Return, 0, 0, true)});
    invoke({});
    numbers({});
}

TEST_F(VarargTest, LateAndRepeatedPrepUsesCurrentTop) {
    chunk(255, 5, {
        abx(Op::Load, 0, 99, true), // late prep reads the modified value
        abx(Op::VargPrep, 1), abx(Op::Varg, 1, 0, true),
        abx(Op::VargPrep, 2), abx(Op::Varg, 2, 0, true),
        abx(Op::Return, 0, 0, true)});
    invoke({10, 20, 30});
    numbers({99, 20, 30});
}

TEST_F(VarargTest, PrepCanExecuteInFixedArityChunk) {
    chunk(3, 3, {abx(Op::VargPrep, 1), abx(Op::Varg, 1, 0, true), abx(Op::Return, 0, 0, true)});
    invoke({10, 20, 30, 40});
    numbers({10, 20, 30});
}

TEST_F(VarargTest, ForwardingNestedOpenCallsBeyondFixedSlots) {
    chunk(255, 2, {abx(Op::VargPrep, 0), abx(Op::Closure, 0, 1),
                   abx(Op::Varg, 1, 0, true), abc(Op::Call, 0, 0, VM::multret, true),
                   abx(Op::Return, 0, 0, true)});
    chunk(255, 1, {abx(Op::VargPrep, 0), abx(Op::Varg, 0, 0, true), abx(Op::Return, 0, 0, true)});
    vm.push_value(Value::closure(vm.make_closure(cu, 0)));
    for (unsigned i = 0; i < 600; ++i) vm.push_value(Value::number(i));
    vm.call(600, VM::multret);
    ASSERT_EQ(vm.stack_top(), 600U);
    for (unsigned i = 0; i < 600; ++i) EXPECT_EQ(vm.getlocal(i).number_, i);
}

TEST_F(VarargTest, FixedReturnContractPadsAndTruncates) {
    chunk(0, 4, {abx(Op::Closure, 0, 1), abc(Op::Call, 0, 0, 4), abx(Op::Return, 0, 0, true)});
    chunk(0, 2, {abx(Op::Load, 0, 10, true), abx(Op::Load, 1, 20, true), abx(Op::Return, 0, 2)});
    invoke({});
    ASSERT_EQ(vm.stack_top(), 4U);
    EXPECT_EQ(vm.getlocal(0).number_, 10);
    EXPECT_EQ(vm.getlocal(1).number_, 20);
    EXPECT_EQ(vm.getlocal(2).kind, ValueKind::Nil);
    EXPECT_EQ(vm.getlocal(3).kind, ValueKind::Nil);
    while (vm.stack_top()) (void)vm.pop_value();
    invoke({}, 1);
    numbers({10});
}

TEST_F(VarargTest, EmptyMultretDoesNotReturnAllocatedSlots) {
    chunk(0, 8, {abx(Op::Closure, 0, 1), abc(Op::Call, 0, 0, VM::multret), abx(Op::Return, 0, 0, true)});
    chunk(0, 8, {abx(Op::Return, 0, 0)});
    invoke({});
    numbers({});
}

TEST_F(VarargTest, CFunctionReceivesAndReturnsMoreThan255Values) {
    auto* echo = vm.make_closure_c([](VM* vm) {
        const auto argc = static_cast<std::uint32_t>(vm->stack_top());
        for (std::uint32_t i = 0; i < argc; ++i) vm->push_value(vm->getlocal(i));
    }, 0);
    cu->constants_.push_back(Value::closure(echo));
    chunk(255, 2, {abx(Op::VargPrep, 0), abx(Op::LoadK, 0, 0), abx(Op::Varg, 1, 0, true),
                   abc(Op::Call, 0, 0, VM::multret, true), abx(Op::Return, 0, 0, true)});
    vm.push_value(Value::closure(vm.make_closure(cu, 0)));
    for (unsigned i = 0; i < 300; ++i) vm.push_value(Value::number(i));
    vm.call(300, VM::multret);
    ASSERT_EQ(vm.stack_top(), 300U);
    EXPECT_EQ(vm.getlocal(299).number_, 299);
}

TEST_F(VarargTest, ReentrantCFunctionPreservesArgumentsAndResults) {
    chunk(255, 1, {abx(Op::VargPrep, 0), abx(Op::Varg, 0, 0, true), abx(Op::Return, 0, 0, true)});
    auto* forward = vm.make_closure_c([](VM* vm) {
        vm->push_value(vm->getlocal(0));
        vm->push_value(Value::number(10));
        vm->push_value(Value::number(20));
        vm->call(2, VM::multret);
        EXPECT_EQ(vm->stack_top(), 3U); // original closure arg plus two results
        EXPECT_EQ(vm->getlocal(0).kind, ValueKind::Closure);
    }, 0);
    vm.push_value(Value::closure(forward));
    vm.push_value(Value::closure(vm.make_closure(cu, 0)));
    vm.call(1, VM::multret);
    numbers({10, 20});
}

TEST_F(VarargTest, OldCapturedRegistersCannotOverwriteNewFunctionSlot) {
    const Value key = Value::string(vm.make_string("escaped"));
    cu->constants_.push_back(key);
    chunk(255, 5, {
        abx(Op::Closure, 4, 1), abx(Op::SetGlobalK, 0, 4),
        abx(Op::VargPrep, 1), abx(Op::GetGlobalK, 0, 1), abc(Op::Call, 1, 0, 0),
        abx(Op::Varg, 1, 0, true), abx(Op::Return, 0, 0, true)});
    chunk(0, 1, {abx(Op::SetUpvalue, 0, 42, true), abx(Op::GetUpvalue, 0, 0), abx(Op::Return, 0, 1)},
          {{UpvalueSource::Local, 3}});
    invoke({10, 20, 30});
    numbers({10, 20, 30});
    while (vm.stack_top()) (void)vm.pop_value();
    Value escaped;
    ASSERT_TRUE(vm.globals()->get(key, &escaped));
    ASSERT_FALSE(escaped.closure_->at(0)->is_open);
    vm.push_value(escaped);
    vm.call(0, 1);
    numbers({42});
}

TEST_F(VarargTest, EscapingClosureCapturesFinalFixedRegister) {
    chunk(255, 2, {abx(Op::VargPrep, 1), abx(Op::Closure, 1, 1), abx(Op::Return, 1, 1)});
    chunk(0, 1, {abx(Op::GetUpvalue, 0, 0), abx(Op::Return, 0, 1)}, {{UpvalueSource::Local, 0}});
    invoke({42, 100});
    const auto closure = vm.pop_value();
    vm.push_value(closure);
    vm.call(0, 1);
    numbers({42});
}

TEST_F(VarargTest, InvalidRangesThrowAndVmIsReusable) {
    chunk(255, 1, {abx(Op::VargPrep, 2), abx(Op::Return, 0, 0)});
    EXPECT_THROW(invoke({1, 2}), InvalidCodeError);
    EXPECT_EQ(vm.stack_top(), 0U);
    cu->code_[0] = abx(Op::VargPrep, 1);
    EXPECT_NO_THROW(invoke({1, 2}));
    numbers({});
}

TEST_F(VarargTest, OpenTailDoesNotExpandRegisterNamespace) {
    chunk(255, 1, {abx(Op::VargPrep, 0), abx(Op::Varg, 0, 0, true), abx(Op::Load, 0, 2), abx(Op::Return, 0, 1)});
    EXPECT_THROW(invoke({1, 2, 3}), InvalidCodeError);
}

TEST_F(VarargTest, OpenRangeUnderflowIsRejected) {
    chunk(0, 2, {abc(Op::Call, 0, 0, 0, true)});
    EXPECT_THROW(invoke({}), InvalidCodeError);
    cu->code_[0] = abx(Op::Return, 1, 0, true);
    EXPECT_THROW(invoke({}), InvalidCodeError);
}

TEST_F(VarargTest, RepeatedPrepPreservesValuesAcrossCountCombinations) {
    for (unsigned fixed = 0; fixed <= 8; ++fixed) {
        for (unsigned actual = 0; actual <= 12; ++actual) {
            SCOPED_TRACE(testing::Message() << "fixed=" << fixed << " actual=" << actual);
            VM local;
            auto* code = local.make_code_unit();
            for (unsigned repeat = 0; repeat < 5; ++repeat) {
                code->code_.push_back(abx(Op::VargPrep, fixed));
                code->code_.push_back(abx(Op::Varg, fixed, 0, true));
            }
            code->code_.push_back(abx(Op::Return, 0, 0, true));
            code->chunks_.push_back(Chunk {"matrix", 0, static_cast<std::uint32_t>(code->code_.size()), 255, 8, {}});
            local.push_value(Value::closure(local.make_closure(code, 0)));
            for (unsigned i = 0; i < actual; ++i) local.push_value(Value::number(i));
            local.call(actual, VM::multret);
            ASSERT_EQ(local.stack_top(), std::max(actual, fixed));
            for (unsigned i = 0; i < local.stack_top(); ++i) {
                if (i < actual) EXPECT_EQ(local.getlocal(i).number_, i);
                else EXPECT_EQ(local.getlocal(i).kind, ValueKind::Nil);
            }
        }
    }
}

TEST_F(VarargTest, ExceptionClosesCapturedSlotsFromBeforeRepeatedPrep) {
    const auto key = Value::string(vm.make_string("escaped"));
    cu->constants_.push_back(key);
    chunk(255, 3, {abx(Op::Closure, 2, 1), abx(Op::SetGlobalK, 0, 2),
        abx(Op::VargPrep, 1), abx(Op::VargPrep, 1), abx(static_cast<Op>(63), 0)});
    chunk(0, 1, {abx(Op::GetUpvalue, 0, 0), abx(Op::Return, 0, 1)}, {{UpvalueSource::Local, 0}});
    EXPECT_THROW(invoke({10, 20}), InvalidCodeError);
    EXPECT_EQ(vm.stack_top(), 0U);
    Value escaped;
    ASSERT_TRUE(vm.globals()->get(key, &escaped));
    EXPECT_FALSE(escaped.closure_->at(0)->is_open);
    vm.push_value(escaped);
    vm.call(0, 1);
    ASSERT_EQ(vm.stack_top(), 1U);
    EXPECT_EQ(vm.getlocal(0).kind, ValueKind::Nil);
}

TEST_F(VarargTest, OpenFlagsIgnoreEncodedUnusedOperands) {
    chunk(255, 2, {abx(Op::VargPrep, 0), abx(Op::Closure, 0, 1),
        abx(Op::Varg, 1, 12345, true), abc(Op::Call, 0, 255, VM::multret, true),
        abx(Op::Return, 0, 12345, true)});
    chunk(255, 0, {abx(Op::Return, 0, 9999, true)});
    invoke({10, 20, 30});
    numbers({10, 20, 30});
}

TEST_F(VarargTest, PrepKeepsExistingUpvalueAttachedToItsOriginalExtraSlot) {
    const auto key = Value::string(vm.make_string("extra"));
    cu->constants_.push_back(key);
    // slots==argc lets prep keep extras in place. A captured extra remains shared.
    chunk(255, 3, {abx(Op::Closure, 0, 1), abx(Op::SetGlobalK, 0, 0),
        abx(Op::VargPrep, 1), abc(Op::Call, 0, 0, 0), abx(Op::Varg, 0, 0, true),
        abx(Op::Return, 0, 0, true)});
    chunk(0, 0, {abx(Op::SetUpvalue, 0, 99, true), abx(Op::Return, 0, 0)},
          {{UpvalueSource::Local, 1}});
    invoke({10, 20, 30});
    numbers({99, 30});
}
} // namespace

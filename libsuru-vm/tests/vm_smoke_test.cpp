#include "suru/vm/vm.hpp"

#include <gtest/gtest.h>

TEST(VmSmokeTest, ExecutesSimpleAddProgram) {
    suru::vm::BytecodeModule module;
    module.instructions = {
        {suru::vm::Opcode::ConstI64, 2},
        {suru::vm::Opcode::ConstI64, 3},
        {suru::vm::Opcode::AddI64, 0},
        {suru::vm::Opcode::Halt, 0},
    };

    auto result = suru::vm::execute(module);
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_EQ(result.final_stack.size(), 1U);
    EXPECT_EQ(result.final_stack.back(), 5);
}


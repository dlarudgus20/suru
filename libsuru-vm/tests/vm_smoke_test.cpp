#include "suru/vm/vm.hpp"

#include <iostream>

int main() {
    suru::vm::BytecodeModule module;
    module.instructions = {
        {suru::vm::Opcode::ConstI64, 2},
        {suru::vm::Opcode::ConstI64, 3},
        {suru::vm::Opcode::AddI64, 0},
        {suru::vm::Opcode::Halt, 0},
    };

    auto result = suru::vm::execute(module);
    if (result.exit_code != 0) {
        std::cerr << "execution failed: " << result.error_message << '\n';
        return 1;
    }
    if (result.final_stack.size() != 1 || result.final_stack.back() != 5) {
        std::cerr << "unexpected stack result\n";
        return 1;
    }

    return 0;
}

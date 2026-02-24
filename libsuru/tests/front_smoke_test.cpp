#include "suru/front/compiler.hpp"

#include <iostream>

int main() {
    auto result = suru::front::compile("return 1 + 2 + 3");
    if (!result.ok()) {
        std::cerr << "compile unexpectedly failed\n";
        return 1;
    }

    if (result.module.instructions.empty()) {
        std::cerr << "no instructions emitted\n";
        return 1;
    }

    if (result.module.instructions.back().opcode != suru::vm::Opcode::Halt) {
        std::cerr << "module must terminate with HALT\n";
        return 1;
    }

    return 0;
}

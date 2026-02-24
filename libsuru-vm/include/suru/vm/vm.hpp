#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "suru/vm/bytecode.hpp"

namespace suru::vm {

struct VMOptions {
    std::size_t max_stack {1024};
    bool trace {false};
    std::ostream* out {nullptr};
};

struct ExecutionResult {
    int exit_code {0};
    std::string error_message;
    std::vector<std::int64_t> final_stack;
};

ExecutionResult execute(const BytecodeModule& module, const VMOptions& options = {});

} // namespace suru::vm

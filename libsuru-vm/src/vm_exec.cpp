#include "suru/vm/vm.hpp"

namespace suru::vm {

void VM::exec_call() {
    if (!i_stack_.empty()) {
        throw std::runtime_error("");
    }
    if (v_stack_.empty()) {
        throw std::runtime_error("");
    }
    auto f = v_stack_.back();
    if (f.kind != ValueKind::Closure) {
        throw std::runtime_error("");
    }
    v_stack_.pop_back();
    i_stack_.emplace_back(f.closure_, 0);
    Closure* current = i_stack_.back().first;
    if (current->code == nullptr) {
        current->cfunc(this, current);
        return;
    }
    size_t& pc = i_stack_.back().second;
    const Chunk& chunk = current->code->chunks_[current->chunk_index];
    while (pc < chunk.opcodes_.size()) {
        // TODO
        pc++;
    }
}

} // namespace suru::vm

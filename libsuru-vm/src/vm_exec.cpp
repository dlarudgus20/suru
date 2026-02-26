#include "suru/vm/vm.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "suru/vm/opcode.hpp"

namespace suru::vm {
namespace {

bool is_falsey(Value value) {
    return value.kind == ValueKind::Nil || (value.kind == ValueKind::Boolean && !value.bool_);
}

double require_number(Value value, std::string_view where) {
    if (value.kind != ValueKind::Number) {
        throw std::runtime_error(std::string(where) + ": expected number");
    }
    return value.number_;
}

Table* require_table(Value value, std::string_view where) {
    if (value.kind != ValueKind::Table || value.table_ == nullptr) {
        throw std::runtime_error(std::string(where) + ": expected table");
    }
    return value.table_;
}

Closure* require_closure(Value value, std::string_view where) {
    if (value.kind != ValueKind::Closure || value.closure_ == nullptr) {
        throw std::runtime_error(std::string(where) + ": expected closure");
    }
    return value.closure_;
}

std::int64_t require_integer(Value value, std::string_view where) {
    const double number = require_number(value, where);
    return static_cast<std::int64_t>(number);
}

std::uint64_t read_uleb(const CodeUnit& cu, std::size_t code_end, std::size_t& pc) {
    std::uint64_t value = 0;
    int shift = 0;
    while (true) {
        if (pc >= code_end || pc >= cu.opcodes_.size()) {
            throw std::runtime_error("truncated uleb128");
        }
        const std::uint8_t byte = cu.opcodes_[pc++];
        value |= static_cast<std::uint64_t>(byte & 0x7fU) << shift;
        if ((byte & 0x80U) == 0U) {
            return value;
        }
        shift += 7;
        if (shift > 63) {
            throw std::runtime_error("uleb128 overflow");
        }
    }
}

std::int64_t read_sleb(const CodeUnit& cu, std::size_t code_end, std::size_t& pc) {
    std::int64_t value = 0;
    int shift = 0;
    std::uint8_t byte = 0;
    while (true) {
        if (pc >= code_end || pc >= cu.opcodes_.size()) {
            throw std::runtime_error("truncated sleb128");
        }
        byte = cu.opcodes_[pc++];
        value |= static_cast<std::int64_t>(byte & 0x7fU) << shift;
        shift += 7;
        if ((byte & 0x80U) == 0U) {
            break;
        }
        if (shift > 63) {
            throw std::runtime_error("sleb128 overflow");
        }
    }

    if (shift < 64 && (byte & 0x40U) != 0U) {
        value |= ~((static_cast<std::int64_t>(1) << shift) - 1);
    }
    return value;
}

void push_results(std::vector<Value>& stack, const std::vector<Value>& results, std::size_t expected) {
    if (expected == 0) {
        return;
    }

    const std::size_t emit = (results.size() < expected) ? results.size() : expected;
    for (std::size_t i = 0; i < emit; ++i) {
        stack.push_back(results[i]);
    }
    for (std::size_t i = emit; i < expected; ++i) {
        stack.push_back(Value::nil());
    }
}

} // namespace

void VM::make_call_frame(std::size_t arg_count, std::size_t ret_slots) {
    if (v_stack_.size() < arg_count + 1U) {
        throw std::runtime_error("call stack underflow");
    }

    const std::size_t callee_index = v_stack_.size() - arg_count - 1U;
    Closure* callee = require_closure(v_stack_[callee_index], "call");

    if (callee->code == nullptr) {
        for (std::size_t i = 0; i < arg_count; ++i) {
            v_stack_[callee_index + i] = v_stack_[callee_index + 1U + i];
        }
        v_stack_.resize(v_stack_.size() - 1U);
        i_stack_.push_back(CallFrame {callee, 0, callee_index, 0, ret_slots});
        return;
    }

    if (callee->chunk_index >= callee->code->chunks_.size()) {
        throw std::runtime_error("callee chunk index out of bounds");
    }
    const Chunk& callee_chunk = callee->code->chunks_[callee->chunk_index];
    if (callee_chunk.arity > callee_chunk.slots) {
        throw std::runtime_error("chunk arity exceeds slots");
    }

    std::vector<Value> params;
    params.reserve(callee_chunk.arity);
    const std::size_t copied = (arg_count < callee_chunk.arity) ? arg_count : callee_chunk.arity;
    for (std::size_t i = 0; i < copied; ++i) {
        params.push_back(v_stack_[callee_index + 1U + i]);
    }
    for (std::size_t i = copied; i < callee_chunk.arity; ++i) {
        params.push_back(Value::nil());
    }

    v_stack_.resize(callee_index);
    for (Value value : params) {
        v_stack_.push_back(value);
    }
    v_stack_.resize(callee_index + callee_chunk.slots, Value::nil());
    i_stack_.push_back(CallFrame {
        callee,
        callee_chunk.code_begin,
        callee_index,
        callee_chunk.code_end,
        ret_slots,
    });
}

void VM::run_c_frame() {
    Closure* current = i_stack_.back().closure;
    if (current == nullptr || current->code != nullptr) {
        throw std::runtime_error("run_c_frame called with non-c frame");
    }
    if (current->cfunc == nullptr) {
        throw std::runtime_error("c closure has null function");
    }

    const std::size_t frame_base = i_stack_.back().base;
    const std::size_t expected = i_stack_.back().ret_slots;
    const std::size_t produced_base = v_stack_.size();

    current->cfunc(this);
    // i_stack_ can be reallocated by nested vm.call().
    // thus a reference to i_stack_.back() is dangerous.

    if (v_stack_.size() < frame_base) {
        throw std::runtime_error("frame stack underflow");
    }

    std::size_t result_begin = produced_base;
    if (v_stack_.size() < produced_base) {
        // C function popped into its argument/local area.
        result_begin = frame_base;
    }

    std::vector<Value> produced;
    produced.reserve(v_stack_.size() - result_begin);
    for (std::size_t i = result_begin; i < v_stack_.size(); ++i) {
        produced.push_back(v_stack_[i]);
    }

    i_stack_.pop_back();
    v_stack_.resize(frame_base);
    push_results(v_stack_, produced, expected);
}

void VM::run(std::size_t target_depth) {
    while (i_stack_.size() > target_depth) {
        CallFrame& frame = i_stack_.back();
        Closure* current = frame.closure;
        if (current == nullptr) {
            throw std::runtime_error("invalid target depth");
        }

        if (current->code == nullptr) {
            run_c_frame();
            continue;
        }

        CodeUnit* cu = current->code;
        if (frame.pc >= frame.code_end) {
            i_stack_.pop_back();
            const std::size_t expected = frame.ret_slots;
            const std::size_t base_to_restore = frame.base;
            v_stack_.resize(base_to_restore);
            push_results(v_stack_, {}, expected);
            continue;
        }

        if (frame.pc >= cu->opcodes_.size()) {
            throw std::runtime_error("program counter out of bytecode bounds");
        }

        const Op op = static_cast<Op>(cu->opcodes_[frame.pc++]);
        switch (op) {
            case Op::Pop: {
                (void)pop_value();
                break;
            }
            case Op::Nil: v_stack_.push_back(Value::nil()); break;
            case Op::True: v_stack_.push_back(Value::boolean(true)); break;
            case Op::False: v_stack_.push_back(Value::boolean(false)); break;
            case Op::Const: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (idx >= cu->constants_.size()) {
                    throw std::runtime_error("constant index out of bounds");
                }
                v_stack_.push_back(cu->constants_[idx]);
                break;
            }
            case Op::GetLocal: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                const std::size_t slot = frame.base + idx;
                if (slot >= v_stack_.size()) {
                    throw std::runtime_error("local slot out of bounds");
                }
                v_stack_.push_back(v_stack_[slot]);
                break;
            }
            case Op::SetLocal: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                const std::size_t slot = frame.base + idx;
                if (slot >= v_stack_.size()) {
                    throw std::runtime_error("local slot out of bounds");
                }
                v_stack_[slot] = pop_value();
                break;
            }
            case Op::GetGlobal: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (idx >= cu->constants_.size()) {
                    throw std::runtime_error("global key constant index out of bounds");
                }
                const Value key = cu->constants_[idx];
                if (key.kind != ValueKind::String) {
                    throw std::runtime_error("global key must be string");
                }
                Value out = Value::nil();
                if (!globals()->get(key, &out)) {
                    out = Value::nil();
                }
                v_stack_.push_back(out);
                break;
            }
            case Op::SetGlobal: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (idx >= cu->constants_.size()) {
                    throw std::runtime_error("global key constant index out of bounds");
                }
                const Value key = cu->constants_[idx];
                if (key.kind != ValueKind::String) {
                    throw std::runtime_error("global key must be string");
                }
                const Value value = pop_value();
                if (!globals()->set(key, value)) {
                    throw std::runtime_error("failed to set global value");
                }
                break;
            }
            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Div:
            case Op::Idiv:
            case Op::Mod:
            case Op::Pow: {
                const Value rhs_v = pop_value();
                const Value lhs_v = pop_value();
                const double lhs = require_number(lhs_v, "binary op");
                const double rhs = require_number(rhs_v, "binary op");
                double result = 0.0;
                switch (op) {
                    case Op::Add: result = lhs + rhs; break;
                    case Op::Sub: result = lhs - rhs; break;
                    case Op::Mul: result = lhs * rhs; break;
                    case Op::Div: result = lhs / rhs; break;
                    case Op::Idiv: result = std::floor(lhs / rhs); break;
                    case Op::Mod: result = std::fmod(lhs, rhs); break;
                    case Op::Pow: result = std::pow(lhs, rhs); break;
                    default: break;
                }
                v_stack_.push_back(Value::number(result));
                break;
            }
            case Op::Neg: {
                const double v = require_number(pop_value(), "neg");
                v_stack_.push_back(Value::number(-v));
                break;
            }
            case Op::Not: {
                v_stack_.push_back(Value::boolean(is_falsey(pop_value())));
                break;
            }
            case Op::Eq:
            case Op::Ne: {
                const Value rhs = pop_value();
                const Value lhs = pop_value();
                const bool eq = value_equals(lhs, rhs);
                v_stack_.push_back(Value::boolean(op == Op::Eq ? eq : !eq));
                break;
            }
            case Op::Lt:
            case Op::Le:
            case Op::Gt:
            case Op::Ge: {
                const double rhs = require_number(pop_value(), "compare");
                const double lhs = require_number(pop_value(), "compare");
                bool result = false;
                switch (op) {
                    case Op::Lt: result = lhs < rhs; break;
                    case Op::Le: result = lhs <= rhs; break;
                    case Op::Gt: result = lhs > rhs; break;
                    case Op::Ge: result = lhs >= rhs; break;
                    default: break;
                }
                v_stack_.push_back(Value::boolean(result));
                break;
            }
            case Op::Band:
            case Op::Bor:
            case Op::Bxor:
            case Op::Shl:
            case Op::Shr: {
                const std::int64_t rhs = require_integer(pop_value(), "bit op");
                const std::int64_t lhs = require_integer(pop_value(), "bit op");
                std::int64_t result = 0;
                switch (op) {
                    case Op::Band: result = lhs & rhs; break;
                    case Op::Bor: result = lhs | rhs; break;
                    case Op::Bxor: result = lhs ^ rhs; break;
                    case Op::Shl: result = lhs << rhs; break;
                    case Op::Shr: result = lhs >> rhs; break;
                    default: break;
                }
                v_stack_.push_back(Value::number(static_cast<double>(result)));
                break;
            }
            case Op::NewTable: {
                v_stack_.push_back(Value::table(make_table()));
                break;
            }
            case Op::GetTable: {
                const Value key = pop_value();
                const Value table_v = pop_value();
                Table* table = require_table(table_v, "gettable");
                Value out = Value::nil();
                if (!table->get(key, &out)) {
                    out = Value::nil();
                }
                v_stack_.push_back(out);
                break;
            }
            case Op::SetTable: {
                const Value value = pop_value();
                const Value key = pop_value();
                const Value table_v = pop_value();
                Table* table = require_table(table_v, "settable");
                if (!table->set(key, value)) {
                    throw std::runtime_error("failed to set table key");
                }
                break;
            }
            case Op::Jmp: {
                const std::int64_t rel = read_sleb(*cu, frame.code_end, frame.pc);
                const std::int64_t next = static_cast<std::int64_t>(frame.pc) + rel;
                if (next < 0 || static_cast<std::size_t>(next) > frame.code_end) {
                    throw std::runtime_error("jump target out of bounds");
                }
                frame.pc = static_cast<std::size_t>(next);
                break;
            }
            case Op::JmpIfFalse: {
                const std::int64_t rel = read_sleb(*cu, frame.code_end, frame.pc);
                const Value cond = pop_value();
                if (is_falsey(cond)) {
                    const std::int64_t next = static_cast<std::int64_t>(frame.pc) + rel;
                    if (next < 0 || static_cast<std::size_t>(next) > frame.code_end) {
                        throw std::runtime_error("jump target out of bounds");
                    }
                    frame.pc = static_cast<std::size_t>(next);
                }
                break;
            }
            case Op::Call: {
                const std::size_t arg_count = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                const std::size_t ret_count = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                make_call_frame(arg_count, ret_count);
                break;
            }
            case Op::Closure: {
                const std::size_t chunk_index = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (chunk_index >= cu->chunks_.size()) {
                    throw std::runtime_error("closure chunk index out of bounds");
                }
                const Chunk& chunk = cu->chunks_[chunk_index];
                Closure* closure = make_closure(cu, chunk_index, chunk.upvalues);
                if (stack_top() < chunk.upvalues) {
                    throw std::runtime_error("upvalue capture stack underflow");
                }
                for (std::size_t i = chunk.upvalues; i > 0; --i) {
                    closure->at(i - 1U) = pop_value();
                }
                v_stack_.push_back(Value::closure(closure));
                break;
            }
            case Op::GetUpvalue: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (idx >= current->len) {
                    throw std::runtime_error("upvalue index out of bounds");
                }
                v_stack_.push_back(current->at(idx));
                break;
            }
            case Op::SetUpvalue: {
                const std::size_t idx = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (idx >= current->len) {
                    throw std::runtime_error("upvalue index out of bounds");
                }
                current->at(idx) = pop_value();
                break;
            }
            case Op::Return: {
                const std::size_t ret_count = static_cast<std::size_t>(read_uleb(*cu, frame.code_end, frame.pc));
                if (v_stack_.size() < ret_count) {
                    throw std::runtime_error("return stack underflow");
                }

                std::vector<Value> results(ret_count);
                for (std::size_t i = 0; i < ret_count; ++i) {
                    results[ret_count - 1U - i] = pop_value();
                }

                const std::size_t frame_base = frame.base;
                const std::size_t expected = frame.ret_slots;
                i_stack_.pop_back();
                v_stack_.resize(frame_base);
                push_results(v_stack_, results, expected);
                break;
            }
            default:
                throw std::runtime_error("unknown opcode");
        }
    }
}

void VM::call(std::size_t arg_count, std::size_t ret_slots) {
    if (i_stack_.empty()) {
        throw std::runtime_error("call stack is not initialized");
    }

    const std::size_t caller_depth = i_stack_.size();
    make_call_frame(arg_count, ret_slots);
    run(caller_depth);
}

} // namespace suru::vm

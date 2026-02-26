#include "suru/vm/vm.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/opcode.hpp"

namespace suru::vm {
namespace {

constexpr std::uint32_t kOpShift = 26U;
constexpr std::uint32_t kOpMask = 0x3FU;
constexpr std::uint32_t kAShift = 18U;
constexpr std::uint32_t kAMask = 0xFFU;
constexpr std::uint32_t kBShift = 9U;
constexpr std::uint32_t kBMask = 0x1FFU;
constexpr std::uint32_t kCMask = 0x1FFU;
constexpr std::uint32_t kBxMask = 0x3FFFFU;
constexpr std::uint32_t kAxMask = 0x3FFFFFFU;

bool is_falsey(Value value) {
    return value.kind == ValueKind::Nil || (value.kind == ValueKind::Boolean && !value.bool_);
}

double require_number(Value value, std::string_view where) {
    if (value.kind != ValueKind::Number) {
        throw std::runtime_error(std::string(where) + ": expected number");
    }
    return value.number_;
}

bool require_boolean(Value value, std::string_view where) {
    if (value.kind != ValueKind::Boolean) {
        throw std::runtime_error(std::string(where) + ": expected boolean");
    }
    return value.bool_;
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

std::uint32_t to_u32(std::size_t value, std::string_view where) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string(where) + ": exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint8_t to_u8(std::uint32_t value, std::string_view where) {
    if (value > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error(std::string(where) + ": exceeds uint8 range");
    }
    return static_cast<std::uint8_t>(value);
}

Op decode_op(std::uint32_t word) {
    return static_cast<Op>((word >> kOpShift) & kOpMask);
}

std::uint32_t decode_a(std::uint32_t word) {
    return (word >> kAShift) & kAMask;
}

std::uint32_t decode_b(std::uint32_t word) {
    return (word >> kBShift) & kBMask;
}

std::uint32_t decode_c(std::uint32_t word) {
    return word & kCMask;
}

std::uint32_t decode_bx(std::uint32_t word) {
    return word & kBxMask;
}

std::int32_t decode_sax(std::uint32_t word) {
    const std::uint32_t raw = word & kAxMask;
    if ((raw & (1U << 25U)) == 0U) {
        return static_cast<std::int32_t>(raw);
    }
    return static_cast<std::int32_t>(raw | (~kAxMask));
}

Value reg_read(std::uint32_t base, std::uint32_t limit, const std::vector<Value>& stack, std::uint32_t index) {
    const std::uint32_t at32 = base + index;
    if (at32 >= limit) {
        throw std::runtime_error("register index out of bounds");
    }
    const std::size_t at = static_cast<std::size_t>(at32);
    if (at >= stack.size()) {
        throw std::runtime_error("register read out of stack bounds");
    }
    return stack[at];
}

void reg_write(std::uint32_t base, std::uint32_t limit, std::vector<Value>& stack, std::uint32_t index, Value value) {
    const std::uint32_t at32 = base + index;
    if (at32 >= limit) {
        throw std::runtime_error("register index out of bounds");
    }
    const std::size_t at = static_cast<std::size_t>(at32);
    if (at >= stack.size()) {
        throw std::runtime_error("register write out of stack bounds");
    }
    stack[at] = value;
}

void skip_next_word(std::uint32_t& pc, std::uint32_t code_end) {
    if (pc >= code_end) {
        throw std::runtime_error("skip target out of bounds");
    }
    ++pc;
}

} // namespace

void VM::finish_frame_return(
    std::uint32_t frame_base,
    std::uint8_t expected,
    std::uint32_t result_begin,
    std::uint32_t result_end
) {
    if (i_stack_.empty()) {
        throw std::runtime_error("call stack is not initialized");
    }
    if (result_begin > result_end || static_cast<std::size_t>(result_end) > v_stack_.size()) {
        throw std::runtime_error("invalid return result range");
    }

    CallFrame& caller = i_stack_.back();
    const std::uint32_t available = result_end - result_begin;
    const std::uint8_t emit = (available < expected) ? static_cast<std::uint8_t>(available) : expected;

    if (caller.closure != nullptr && caller.closure->code != nullptr) {
        if (caller.call_retc != expected) {
            throw std::runtime_error("call return count mismatch");
        }
        const std::uint32_t caller_limit = frame_base;
        if (caller.base + caller.call_dst + expected > caller_limit) {
            throw std::runtime_error("call return range out of bounds");
        }
        for (std::uint8_t i = 0; i < emit; ++i) {
            reg_write(caller.base, caller_limit, v_stack_, static_cast<std::uint32_t>(caller.call_dst + i), v_stack_[result_begin + i]);
        }
        for (std::uint8_t i = emit; i < expected; ++i) {
            reg_write(caller.base, caller_limit, v_stack_, static_cast<std::uint32_t>(caller.call_dst + i), Value::nil());
        }
        v_stack_.resize(frame_base);
        caller.call_dst = 0;
        caller.call_retc = 0;
        return;
    }

    for (std::uint8_t i = 0; i < emit; ++i) {
        v_stack_[frame_base + i] = v_stack_[result_begin + i];
    }
    v_stack_.resize(frame_base + expected);
    for (std::uint8_t i = emit; i < expected; ++i) {
        v_stack_[frame_base + i] = Value::nil();
    }
}

void VM::make_call_frame(std::uint8_t arg_count, std::uint8_t ret_slots) {
    if (v_stack_.size() < static_cast<std::size_t>(arg_count) + 1U) {
        throw std::runtime_error("call stack underflow");
    }

    const std::size_t callee_index = v_stack_.size() - static_cast<std::size_t>(arg_count) - 1U;
    Closure* callee = require_closure(v_stack_[callee_index], "call");

    if (callee->code == nullptr) {
        for (std::uint8_t i = 0; i < arg_count; ++i) {
            v_stack_[callee_index + i] = v_stack_[callee_index + 1U + i];
        }
        v_stack_.resize(v_stack_.size() - 1U);
        i_stack_.push_back(CallFrame {callee, 0, to_u32(callee_index, "frame base"), 0, ret_slots, 0, 0});
        return;
    }

    if (callee->chunk_index >= callee->code->chunks_.size()) {
        throw std::runtime_error("callee chunk index out of bounds");
    }
    const Chunk& callee_chunk = callee->code->chunks_[callee->chunk_index];
    if (callee_chunk.arity > callee_chunk.slots) {
        throw std::runtime_error("chunk arity exceeds slots");
    }

    const std::uint8_t copied = (arg_count < callee_chunk.arity) ? arg_count : callee_chunk.arity;
    for (std::uint8_t i = 0; i < copied; ++i) {
        v_stack_[callee_index + i] = v_stack_[callee_index + 1U + i];
    }

    const std::size_t new_size = callee_index + static_cast<std::size_t>(callee_chunk.slots);
    const std::size_t clear_end = (new_size < v_stack_.size()) ? new_size : v_stack_.size();
    for (std::size_t at = callee_index + static_cast<std::size_t>(copied); at < clear_end; ++at) {
        v_stack_[at] = Value::nil();
    }
    v_stack_.resize(new_size, Value::nil());

    i_stack_.push_back(CallFrame {
        callee,
        callee_chunk.code_begin,
        to_u32(callee_index, "frame base"),
        callee_chunk.code_end,
        ret_slots,
        0,
        0,
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

    const std::uint32_t frame_base = i_stack_.back().base;
    const std::uint8_t expected = i_stack_.back().ret_slots;
    const std::size_t produced_base = v_stack_.size();

    current->cfunc(this);

    if (v_stack_.size() < frame_base) {
        throw std::runtime_error("frame stack underflow");
    }

    std::size_t result_begin = produced_base;
    if (v_stack_.size() < produced_base) {
        result_begin = frame_base;
    }

    const std::size_t result_end = v_stack_.size();
    i_stack_.pop_back();
    finish_frame_return(
        frame_base,
        expected,
        to_u32(result_begin, "result begin"),
        to_u32(result_end, "result end")
    );
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
        const std::uint32_t frame_limit = to_u32(v_stack_.size(), "frame limit");
        if (frame.pc >= frame.code_end) {
            throw std::runtime_error("unexpected end of chunk");
        }
        if (static_cast<std::size_t>(frame.pc) >= cu->code_.size()) {
            throw std::runtime_error("program counter out of bytecode bounds");
        }

        const std::uint32_t word = cu->code_[frame.pc++];
        const Op op = decode_op(word);

        switch (op) {
            case Op::Move: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t b = decode_bx(word);
                reg_write(frame.base, frame_limit, v_stack_, a, reg_read(frame.base, frame_limit, v_stack_, b));
                break;
            }
            case Op::LoadNil: {
                reg_write(frame.base, frame_limit, v_stack_, decode_a(word), Value::nil());
                break;
            }
            case Op::LoadTrue: {
                reg_write(frame.base, frame_limit, v_stack_, decode_a(word), Value::boolean(true));
                break;
            }
            case Op::LoadFalse: {
                reg_write(frame.base, frame_limit, v_stack_, decode_a(word), Value::boolean(false));
                break;
            }
            case Op::LoadK: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t k = decode_bx(word);
                if (k >= cu->constants_.size()) {
                    throw std::runtime_error("constant index out of bounds");
                }
                reg_write(frame.base, frame_limit, v_stack_, a, cu->constants_[k]);
                break;
            }
            case Op::GetGlobal: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t k = decode_bx(word);
                if (k >= cu->constants_.size()) {
                    throw std::runtime_error("global key constant index out of bounds");
                }
                const Value key = cu->constants_[k];
                if (key.kind != ValueKind::String) {
                    throw std::runtime_error("global key must be string");
                }
                Value out = Value::nil();
                if (!globals()->get(key, &out)) {
                    out = Value::nil();
                }
                reg_write(frame.base, frame_limit, v_stack_, a, out);
                break;
            }
            case Op::SetGlobal: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t k = decode_bx(word);
                if (k >= cu->constants_.size()) {
                    throw std::runtime_error("global key constant index out of bounds");
                }
                const Value key = cu->constants_[k];
                if (key.kind != ValueKind::String) {
                    throw std::runtime_error("global key must be string");
                }
                if (!globals()->set(key, reg_read(frame.base, frame_limit, v_stack_, a))) {
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
            case Op::Pow:
            case Op::Band:
            case Op::Bor:
            case Op::Bxor:
            case Op::Shl:
            case Op::Shr:
            case Op::Eq:
            case Op::Ne:
            case Op::Lt:
            case Op::Le:
            case Op::Gt:
            case Op::Ge:
            case Op::And:
            case Op::Or: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t b = decode_b(word);
                const std::uint32_t c = decode_c(word);
                const Value lhs_v = reg_read(frame.base, frame_limit, v_stack_, b);
                const Value rhs_v = reg_read(frame.base, frame_limit, v_stack_, c);

                if (op == Op::Eq || op == Op::Ne) {
                    const bool eq = value_equals(lhs_v, rhs_v);
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::boolean(op == Op::Eq ? eq : !eq));
                    break;
                }
                if (op == Op::Lt || op == Op::Le || op == Op::Gt || op == Op::Ge) {
                    const double lhs = require_number(lhs_v, "compare");
                    const double rhs = require_number(rhs_v, "compare");
                    bool result = false;
                    switch (op) {
                        case Op::Lt: result = lhs < rhs; break;
                        case Op::Le: result = lhs <= rhs; break;
                        case Op::Gt: result = lhs > rhs; break;
                        case Op::Ge: result = lhs >= rhs; break;
                        default: break;
                    }
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::boolean(result));
                    break;
                }
                if (op == Op::And || op == Op::Or) {
                    const bool lhs = require_boolean(lhs_v, "logical op");
                    const bool rhs = require_boolean(rhs_v, "logical op");
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::boolean(op == Op::And ? (lhs && rhs) : (lhs || rhs)));
                    break;
                }
                if (op == Op::Band || op == Op::Bor || op == Op::Bxor || op == Op::Shl || op == Op::Shr) {
                    const std::int64_t lhs = require_integer(lhs_v, "bit op");
                    const std::int64_t rhs = require_integer(rhs_v, "bit op");
                    std::int64_t result = 0;
                    switch (op) {
                        case Op::Band: result = lhs & rhs; break;
                        case Op::Bor: result = lhs | rhs; break;
                        case Op::Bxor: result = lhs ^ rhs; break;
                        case Op::Shl: result = lhs << rhs; break;
                        case Op::Shr: result = lhs >> rhs; break;
                        default: break;
                    }
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::number(static_cast<double>(result)));
                    break;
                }

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
                reg_write(frame.base, frame_limit, v_stack_, a, Value::number(result));
                break;
            }
            case Op::Neg:
            case Op::Not: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t b = decode_bx(word);
                const Value input = reg_read(frame.base, frame_limit, v_stack_, b);
                if (op == Op::Neg) {
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::number(-require_number(input, "neg")));
                } else {
                    reg_write(frame.base, frame_limit, v_stack_, a, Value::boolean(is_falsey(input)));
                }
                break;
            }
            case Op::NewTable: {
                reg_write(frame.base, frame_limit, v_stack_, decode_a(word), Value::table(make_table()));
                break;
            }
            case Op::GetTable: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t b = decode_b(word);
                const std::uint32_t c = decode_c(word);
                Table* table = require_table(reg_read(frame.base, frame_limit, v_stack_, b), "gettable");
                const Value key = reg_read(frame.base, frame_limit, v_stack_, c);
                Value out = Value::nil();
                if (!table->get(key, &out)) {
                    out = Value::nil();
                }
                reg_write(frame.base, frame_limit, v_stack_, a, out);
                break;
            }
            case Op::SetTable: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t b = decode_b(word);
                const std::uint32_t c = decode_c(word);
                Table* table = require_table(reg_read(frame.base, frame_limit, v_stack_, a), "settable");
                if (!table->set(reg_read(frame.base, frame_limit, v_stack_, b), reg_read(frame.base, frame_limit, v_stack_, c))) {
                    throw std::runtime_error("failed to set table key");
                }
                break;
            }
            case Op::Jmp: {
                const std::int64_t next = static_cast<std::int64_t>(frame.pc) + decode_sax(word);
                if (next < 0 || static_cast<std::uint64_t>(next) > frame.code_end) {
                    throw std::runtime_error("jump target out of bounds");
                }
                frame.pc = static_cast<std::uint32_t>(next);
                break;
            }
            case Op::IfFalsey: {
                const Value v = reg_read(frame.base, frame_limit, v_stack_, decode_a(word));
                if (!is_falsey(v)) {
                    skip_next_word(frame.pc, frame.code_end);
                }
                break;
            }
            case Op::IfTruthy: {
                const Value v = reg_read(frame.base, frame_limit, v_stack_, decode_a(word));
                if (is_falsey(v)) {
                    skip_next_word(frame.pc, frame.code_end);
                }
                break;
            }
            case Op::IfEq:
            case Op::IfNe:
            case Op::IfLt:
            case Op::IfLe:
            case Op::IfGt:
            case Op::IfGe: {
                const std::uint32_t b = decode_b(word);
                const std::uint32_t c = decode_c(word);
                const Value lhs_v = reg_read(frame.base, frame_limit, v_stack_, b);
                const Value rhs_v = reg_read(frame.base, frame_limit, v_stack_, c);
                bool cond = false;
                if (op == Op::IfEq || op == Op::IfNe) {
                    const bool eq = value_equals(lhs_v, rhs_v);
                    cond = (op == Op::IfEq) ? eq : !eq;
                } else {
                    const double lhs = require_number(lhs_v, "skip compare");
                    const double rhs = require_number(rhs_v, "skip compare");
                    switch (op) {
                        case Op::IfLt: cond = lhs < rhs; break;
                        case Op::IfLe: cond = lhs <= rhs; break;
                        case Op::IfGt: cond = lhs > rhs; break;
                        case Op::IfGe: cond = lhs >= rhs; break;
                        default: break;
                    }
                }
                if (!cond) {
                    skip_next_word(frame.pc, frame.code_end);
                }
                break;
            }
            case Op::Call: {
                const std::uint32_t f = decode_a(word);
                const std::uint32_t arg_count = decode_b(word);
                const std::uint32_t ret_count = decode_c(word);
                if (arg_count > std::numeric_limits<std::uint8_t>::max()) {
                    throw std::runtime_error("call argument count exceeds uint8 range");
                }
                if (ret_count > std::numeric_limits<std::uint8_t>::max()) {
                    throw std::runtime_error("call return count exceeds uint8 range");
                }

                const std::uint32_t avail = frame_limit - frame.base;
                if (f >= avail) {
                    throw std::runtime_error("call register index out of bounds");
                }
                if (f + 1U + arg_count > avail) {
                    throw std::runtime_error("call argument range out of bounds");
                }
                if (f + ret_count > avail) {
                    throw std::runtime_error("call return range out of bounds");
                }

                frame.call_dst = to_u8(f, "call destination register");
                frame.call_retc = to_u8(ret_count, "call return count");
                v_stack_.push_back(reg_read(frame.base, frame_limit, v_stack_, f));
                for (std::uint32_t i = 0; i < arg_count; ++i) {
                    v_stack_.push_back(reg_read(frame.base, frame_limit, v_stack_, f + 1U + i));
                }
                make_call_frame(static_cast<std::uint8_t>(arg_count), static_cast<std::uint8_t>(ret_count));
                break;
            }
            case Op::Closure: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t chunk_index = decode_bx(word);
                if (chunk_index >= cu->chunks_.size()) {
                    throw std::runtime_error("closure chunk index out of bounds");
                }
                const Chunk& chunk = cu->chunks_[chunk_index];
                if (frame.base + a + chunk.upvalues >= frame_limit) {
                    throw std::runtime_error("upvalue capture register range out of bounds");
                }
                Closure* closure = make_closure(cu, chunk_index, chunk.upvalues);
                for (std::uint8_t i = 0; i < chunk.upvalues; ++i) {
                    closure->at(i) = reg_read(frame.base, frame_limit, v_stack_, a + 1U + i);
                }
                reg_write(frame.base, frame_limit, v_stack_, a, Value::closure(closure));
                break;
            }
            case Op::GetUpvalue: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t idx = decode_bx(word);
                if (idx > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(idx) >= current->len) {
                    throw std::runtime_error("upvalue index out of bounds");
                }
                reg_write(frame.base, frame_limit, v_stack_, a, current->at(static_cast<std::uint8_t>(idx)));
                break;
            }
            case Op::SetUpvalue: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t idx = decode_bx(word);
                if (idx > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(idx) >= current->len) {
                    throw std::runtime_error("upvalue index out of bounds");
                }
                current->at(static_cast<std::uint8_t>(idx)) = reg_read(frame.base, frame_limit, v_stack_, a);
                break;
            }
            case Op::Return: {
                const std::uint32_t a = decode_a(word);
                const std::uint32_t ret_count = decode_bx(word);
                if (ret_count > std::numeric_limits<std::uint8_t>::max()) {
                    throw std::runtime_error("return count exceeds uint8 range");
                }
                if (frame.base + a + ret_count > frame_limit) {
                    throw std::runtime_error("return register range out of bounds");
                }

                const std::uint32_t frame_base = frame.base;
                const std::uint8_t expected = frame.ret_slots;
                const std::uint32_t result_begin = frame_base + a;
                const std::uint32_t result_end = result_begin + ret_count;
                i_stack_.pop_back();
                finish_frame_return(frame_base, expected, result_begin, result_end);
                break;
            }
            default:
                throw std::runtime_error("unknown opcode");
        }
    }
}

void VM::call(std::uint8_t arg_count, std::uint8_t ret_slots) {
    if (i_stack_.empty()) {
        throw std::runtime_error("call stack is not initialized");
    }

    const std::size_t caller_depth = i_stack_.size();
    make_call_frame(arg_count, ret_slots);
    run(caller_depth);
}

} // namespace suru::vm

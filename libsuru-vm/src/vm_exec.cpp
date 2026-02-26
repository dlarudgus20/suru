#include "suru/vm/vm.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/error.hpp"
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

std::uint32_t checked_u32_stack_index(std::size_t value, std::string_view where) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw StackOverflowError(std::string(where) + ": exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

Op decode_op(std::uint32_t word) {
    return static_cast<Op>((word >> kOpShift) & kOpMask);
}

struct WordABC {
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
};

struct WordABx {
    std::uint32_t a;
    std::uint32_t bx;
};

struct WordSAx {
    std::int32_t sax;
};

WordABC decode_abc(std::uint32_t word) {
    return WordABC {
        (word >> kAShift) & kAMask,
        (word >> kBShift) & kBMask,
        word & kCMask,
    };
}

WordABx decode_abx(std::uint32_t word) {
    return WordABx {
        (word >> kAShift) & kAMask,
        word & kBxMask,
    };
}

WordSAx decode_sax(std::uint32_t word) {
    const std::uint32_t raw = word & kAxMask;
    if ((raw & (1U << 25U)) == 0U) {
        return WordSAx {static_cast<std::int32_t>(raw)};
    }
    return WordSAx {static_cast<std::int32_t>(raw | (~kAxMask))};
}

} // namespace

Value VM::reg_read(std::uint32_t index) const {
    const CallFrame& frame = i_stack_.back();
    const std::uint32_t base = frame.base;
    const std::uint32_t limit = checked_u32_stack_index(v_stack_.size(), "frame limit");
    const std::uint32_t at32 = base + index;
    if (at32 >= limit) {
        throw InvalidCodeError("register index out of bounds");
    }
    const std::size_t at = static_cast<std::size_t>(at32);
    if (at >= v_stack_.size()) {
        throw InvalidCodeError("register read out of stack bounds");
    }
    return v_stack_[at];
}

void VM::reg_write(std::uint32_t index, Value value) {
    const CallFrame& frame = i_stack_.back();
    const std::uint32_t base = frame.base;
    const std::uint32_t limit = checked_u32_stack_index(v_stack_.size(), "frame limit");
    const std::uint32_t at32 = base + index;
    if (at32 >= limit) {
        throw InvalidCodeError("register index out of bounds");
    }
    const std::size_t at = static_cast<std::size_t>(at32);
    if (at >= v_stack_.size()) {
        throw InvalidCodeError("register write out of stack bounds");
    }
    v_stack_[at] = value;
}

void VM::finish_frame_return(
    std::uint32_t frame_base,
    std::uint8_t expected,
    std::uint32_t result_begin,
    std::uint32_t result_end
) {
    if (result_begin > result_end || static_cast<std::size_t>(result_end) > v_stack_.size()) {
        throw InvalidImageError("invalid return result range");
    }

    CallFrame& caller = i_stack_.back();
    const std::uint32_t available = result_end - result_begin;
    const std::uint8_t emit = (available < expected) ? static_cast<std::uint8_t>(available) : expected;

    if (caller.closure != nullptr && caller.closure->code != nullptr) {
        if (caller.call_retc != expected) {
            throw InternalError("call return count mismatch");
        }
        const std::uint32_t caller_limit = frame_base;
        if (caller.base + caller.call_dst + expected > caller_limit) {
            throw InvalidCodeError("call return range out of bounds");
        }
        for (std::uint8_t i = 0; i < emit; ++i) {
            const std::uint32_t at32 = caller.base + caller.call_dst + i;
            if (at32 >= caller_limit || static_cast<std::size_t>(at32) >= v_stack_.size()) {
                throw InvalidCodeError("call return range out of bounds");
            }
            v_stack_[at32] = v_stack_[result_begin + i];
        }
        for (std::uint8_t i = emit; i < expected; ++i) {
            const std::uint32_t at32 = caller.base + caller.call_dst + i;
            if (at32 >= caller_limit || static_cast<std::size_t>(at32) >= v_stack_.size()) {
                throw InvalidCodeError("call return range out of bounds");
            }
            v_stack_[at32] = Value::nil();
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
        throw InvalidCodeError("call stack underflow");
    }

    const std::size_t callee_index = v_stack_.size() - static_cast<std::size_t>(arg_count) - 1U;
    Closure* callee = v_stack_[callee_index].as_closure("call");

    if (callee->code == nullptr) {
        for (std::uint8_t i = 0; i < arg_count; ++i) {
            v_stack_[callee_index + i] = v_stack_[callee_index + 1U + i];
        }
        v_stack_.resize(v_stack_.size() - 1U);
        i_stack_.push_back(CallFrame {callee, 0, checked_u32_stack_index(callee_index, "frame base"), 0, ret_slots, 0, 0});
        return;
    }

    if (callee->chunk_index >= callee->code->chunks_.size()) {
        throw InvalidImageError("callee chunk index out of bounds");
    }
    const Chunk& callee_chunk = callee->code->chunks_[callee->chunk_index];
    if (callee_chunk.arity > callee_chunk.slots) {
        throw InvalidImageError("chunk arity exceeds slots");
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
        checked_u32_stack_index(callee_index, "frame base"),
        callee_chunk.code_end,
        ret_slots,
        0,
        0,
    });
}

void VM::run_c_frame() {
    Closure* current = i_stack_.back().closure;
    if (current == nullptr || current->code != nullptr) {
        throw InternalError("run_c_frame called with non-c frame");
    }
    if (current->cfunc == nullptr) {
        throw InternalError("c closure has null function");
    }

    const std::uint32_t frame_base = i_stack_.back().base;
    const std::uint8_t expected = i_stack_.back().ret_slots;
    const std::size_t produced_base = v_stack_.size();

    current->cfunc(this);

    if (v_stack_.size() < frame_base) {
        throw InvalidCodeError("frame stack underflow");
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
        checked_u32_stack_index(result_begin, "result begin"),
        checked_u32_stack_index(result_end, "result end")
    );
}

void VM::run(std::size_t target_depth) {
    while (i_stack_.size() > target_depth) {
        CallFrame& frame = i_stack_.back();
        Closure* current = frame.closure;
        if (current == nullptr) {
            throw InternalError("invalid target depth");
        }

        if (current->code == nullptr) {
            run_c_frame();
            continue;
        }

        CodeUnit* cu = current->code;
        const std::uint32_t frame_limit = checked_u32_stack_index(v_stack_.size(), "frame limit");
        if (frame.pc >= frame.code_end) {
            throw InternalError("unexpected end of chunk");
        }
        if (static_cast<std::size_t>(frame.pc) >= cu->code_.size()) {
            throw InvalidCodeError("program counter out of bytecode bounds");
        }

        const std::uint32_t word = cu->code_[frame.pc++];
        const Op op = decode_op(word);

        switch (op) {
            case Op::Move: {
                const auto [a, b] = decode_abx(word);
                reg_write(a, reg_read(b));
                break;
            }
            case Op::LoadNil: {
                const auto a = decode_abx(word).a;
                reg_write(a, Value::nil());
                break;
            }
            case Op::LoadTrue: {
                const auto a = decode_abx(word).a;
                reg_write(a, Value::boolean(true));
                break;
            }
            case Op::LoadFalse: {
                const auto a = decode_abx(word).a;
                reg_write(a, Value::boolean(false));
                break;
            }
            case Op::LoadK: {
                const auto [a, k] = decode_abx(word);
                if (k >= cu->constants_.size()) {
                    throw InvalidCodeError("constant index out of bounds");
                }
                reg_write(a, cu->constants_[k]);
                break;
            }
            case Op::GetGlobal: {
                const auto [a, k] = decode_abx(word);
                if (k >= cu->constants_.size()) {
                    throw InvalidCodeError("global key constant index out of bounds");
                }
                const Value key = cu->constants_[k];
                if (key.kind != ValueKind::String) {
                    throw TableError("global key must be string");
                }
                Value out = Value::nil();
                if (!globals()->get(key, &out)) {
                    out = Value::nil();
                }
                reg_write(a, out);
                break;
            }
            case Op::SetGlobal: {
                const auto [a, k] = decode_abx(word);
                if (k >= cu->constants_.size()) {
                    throw InvalidCodeError("global key constant index out of bounds");
                }
                const Value key = cu->constants_[k];
                if (key.kind != ValueKind::String) {
                    throw TableError("global key must be string");
                }
                if (!globals()->set(key, reg_read(a))) {
                    throw TableError("failed to set global value");
                }
                break;
            }
            case Op::Add: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    reg_read(b).as_number("binary op") + reg_read(c).as_number("binary op")
                ));
                break;
            }
            case Op::Sub: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    reg_read(b).as_number("binary op") - reg_read(c).as_number("binary op")
                ));
                break;
            }
            case Op::Mul: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    reg_read(b).as_number("binary op") * reg_read(c).as_number("binary op")
                ));
                break;
            }
            case Op::Div: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    reg_read(b).as_number("binary op") / reg_read(c).as_number("binary op")
                ));
                break;
            }
            case Op::Idiv: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    std::floor(reg_read(b).as_number("binary op") / reg_read(c).as_number("binary op"))
                ));
                break;
            }
            case Op::Mod: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    std::fmod(reg_read(b).as_number("binary op"), reg_read(c).as_number("binary op"))
                ));
                break;
            }
            case Op::Pow: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    std::pow(reg_read(b).as_number("binary op"), reg_read(c).as_number("binary op"))
                ));
                break;
            }
            case Op::Eq: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    value_equals(reg_read(b), reg_read(c))
                ));
                break;
            }
            case Op::Ne: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    !value_equals(reg_read(b), reg_read(c))
                ));
                break;
            }
            case Op::Lt: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_number("compare") < reg_read(c).as_number("compare")
                ));
                break;
            }
            case Op::Le: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_number("compare") <= reg_read(c).as_number("compare")
                ));
                break;
            }
            case Op::Gt: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_number("compare") > reg_read(c).as_number("compare")
                ));
                break;
            }
            case Op::Ge: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_number("compare") >= reg_read(c).as_number("compare")
                ));
                break;
            }
            case Op::And: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_boolean("logical op") && reg_read(c).as_boolean("logical op")
                ));
                break;
            }
            case Op::Or: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::boolean(
                    reg_read(b).as_boolean("logical op") || reg_read(c).as_boolean("logical op")
                ));
                break;
            }
            case Op::Band: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    static_cast<double>(
                        reg_read(b).as_integer("bit op") & reg_read(c).as_integer("bit op")
                    )
                ));
                break;
            }
            case Op::Bor: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    static_cast<double>(
                        reg_read(b).as_integer("bit op") | reg_read(c).as_integer("bit op")
                    )
                ));
                break;
            }
            case Op::Bxor: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    static_cast<double>(
                        reg_read(b).as_integer("bit op") ^ reg_read(c).as_integer("bit op")
                    )
                ));
                break;
            }
            case Op::Shl: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    static_cast<double>(
                        reg_read(b).as_integer("bit op") << reg_read(c).as_integer("bit op")
                    )
                ));
                break;
            }
            case Op::Shr: {
                const auto [a, b, c] = decode_abc(word);
                reg_write(a, Value::number(
                    static_cast<double>(
                        reg_read(b).as_integer("bit op") >> reg_read(c).as_integer("bit op")
                    )
                ));
                break;
            }
            case Op::Neg: {
                const auto [a, b] = decode_abx(word);
                reg_write(a, Value::number(-reg_read(b).as_number("neg")));
                break;
            }
            case Op::Not: {
                const auto [a, b] = decode_abx(word);
                reg_write(a, Value::boolean(reg_read(b).is_falsy()));
                break;
            }
            case Op::NewTable: {
                const auto a = decode_abx(word).a;
                reg_write(a, Value::table(make_table()));
                break;
            }
            case Op::GetTable: {
                const auto [a, b, c] = decode_abc(word);
                Table* table = reg_read(b).as_table("gettable");
                Value out = Value::nil();
                if (!table->get(reg_read(c), &out)) {
                    out = Value::nil();
                }
                reg_write(a, out);
                break;
            }
            case Op::SetTable: {
                const auto [a, b, c] = decode_abc(word);
                Table* table = reg_read(a).as_table("settable");
                if (!table->set(reg_read(b), reg_read(c))) {
                    throw TableError("failed to set table key");
                }
                break;
            }
            case Op::Jmp: {
                const auto [sax] = decode_sax(word);
                const std::int64_t next = static_cast<std::int64_t>(frame.pc) + sax;
                if (next < 0 || static_cast<std::uint64_t>(next) > frame.code_end) {
                    throw InvalidCodeError("jump target out of bounds");
                }
                frame.pc = static_cast<std::uint32_t>(next);
                break;
            }
            case Op::IfFalsy: {
                const auto a = decode_abx(word).a;
                if (!reg_read(a).is_falsy()) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfTruthy: {
                const auto a = decode_abx(word).a;
                if (reg_read(a).is_falsy()) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfEq: {
                const auto [_, b, c] = decode_abc(word);
                if (!value_equals(reg_read(b), reg_read(c))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfNe: {
                const auto [_, b, c] = decode_abc(word);
                if (value_equals(reg_read(b), reg_read(c))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfLt: {
                const auto [_, b, c] = decode_abc(word);
                if (!(reg_read(b).as_number("if compare")
                    < reg_read(c).as_number("if compare"))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfLe: {
                const auto [_, b, c] = decode_abc(word);
                if (!(reg_read(b).as_number("if compare")
                    <= reg_read(c).as_number("if compare"))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfGt: {
                const auto [_, b, c] = decode_abc(word);
                if (!(reg_read(b).as_number("if compare")
                    > reg_read(c).as_number("if compare"))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfGe: {
                const auto [_, b, c] = decode_abc(word);
                if (!(reg_read(b).as_number("if compare")
                    >= reg_read(c).as_number("if compare"))) {
                    ++frame.pc;
                }
                break;
            }
            case Op::Call: {
                const auto [f, arg_count, ret_count] = decode_abc(word);
                const std::uint32_t avail = frame_limit - frame.base;
                if (f >= avail) {
                    throw InvalidCodeError("call register index out of bounds");
                }
                if (f + 1U + arg_count > avail) {
                    throw InvalidCodeError("call argument range out of bounds");
                }
                if (f + ret_count > avail) {
                    throw InvalidCodeError("call return range out of bounds");
                }

                frame.call_dst = static_cast<std::uint8_t>(f);
                frame.call_retc = static_cast<std::uint8_t>(ret_count);
                v_stack_.push_back(reg_read(f));
                for (std::uint32_t i = 0; i < arg_count; ++i) {
                    v_stack_.push_back(reg_read(f + 1U + i));
                }
                make_call_frame(static_cast<std::uint8_t>(arg_count), static_cast<std::uint8_t>(ret_count));
                break;
            }
            case Op::Closure: {
                const auto [a, chunk_index] = decode_abx(word);
                if (chunk_index >= cu->chunks_.size()) {
                    throw InvalidImageError("closure chunk index out of bounds");
                }
                const Chunk& chunk = cu->chunks_[chunk_index];
                if (frame.base + a + chunk.upvalues >= frame_limit) {
                    throw InvalidCodeError("upvalue capture register range out of bounds");
                }
                Closure* closure = make_closure(cu, chunk_index, chunk.upvalues);
                for (std::uint8_t i = 0; i < chunk.upvalues; ++i) {
                    closure->at(i) = reg_read(a + 1U + i);
                }
                reg_write(a, Value::closure(closure));
                break;
            }
            case Op::GetUpvalue: {
                const auto [a, idx] = decode_abx(word);
                if (idx > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(idx) >= current->len) {
                    throw InvalidCodeError("upvalue index out of bounds");
                }
                reg_write(a, current->at(static_cast<std::uint8_t>(idx)));
                break;
            }
            case Op::SetUpvalue: {
                const auto [a, idx] = decode_abx(word);
                if (idx > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(idx) >= current->len) {
                    throw InvalidCodeError("upvalue index out of bounds");
                }
                current->at(static_cast<std::uint8_t>(idx)) = reg_read(a);
                break;
            }
            case Op::Return: {
                const auto [a, ret_count] = decode_abx(word);
                if (frame.base + a + ret_count > frame_limit) {
                    throw InvalidCodeError("return register range out of bounds");
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
                throw InvalidCodeError("unknown opcode");
        }
    }
}

void VM::call(std::uint8_t arg_count, std::uint8_t ret_slots) {
    if (i_stack_.empty()) {
        throw InternalError("call stack is not initialized");
    }

    const std::size_t caller_depth = i_stack_.size();
    make_call_frame(arg_count, ret_slots);
    run(caller_depth);
}

} // namespace suru::vm

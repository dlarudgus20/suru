#include "suru/vm/vm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/error.hpp"
#include "suru/vm/opcode.hpp"

namespace suru::vm {
namespace {

constexpr std::uint32_t kOpShift = 26U;
constexpr std::uint32_t kOpMask = 0x3FU;
constexpr std::uint32_t kIShift = 25U;
constexpr std::uint32_t kIMask = 0x1U;
constexpr std::uint32_t kAShift = 17U;
constexpr std::uint32_t kAMask = 0xFFU;
constexpr std::uint32_t kBShift = 9U;
constexpr std::uint32_t kBMask = 0xFFU;
constexpr std::uint32_t kCMask = 0x1FFU;
constexpr std::uint32_t kBxMask = 0x1FFFFU;
constexpr std::uint32_t kAxMask = 0x1FFFFFFU;

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
    bool i;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;

    std::int32_t imm_c() const {
        const std::uint32_t raw = c & kCMask;
        if ((raw & (1U << 8U)) == 0U) {
            return static_cast<std::int32_t>(raw);
        }
        return static_cast<std::int32_t>(raw | (~kCMask));
    }

    std::int32_t imm_b() const {
        const std::uint32_t raw = b & kBMask;
        if ((raw & (1U << 7U)) == 0U) {
            return static_cast<std::int32_t>(raw);
        }
        return static_cast<std::int32_t>(raw | (~kBMask));
    }
};

struct WordABx {
    bool i;
    std::uint32_t a;
    std::uint32_t bx;

    std::int32_t imm_bx() const {
        const std::uint32_t raw = bx & kBxMask;
        if ((raw & (1U << 16U)) == 0U) {
            return static_cast<std::int32_t>(raw);
        }
        return static_cast<std::int32_t>(raw | (~kBxMask));
    }
};

struct WordSAx {
    std::int32_t sax;
};

WordABC decode_abc(std::uint32_t word) {
    return WordABC {
        ((word >> kIShift) & kIMask) != 0U,
        (word >> kAShift) & kAMask,
        (word >> kBShift) & kBMask,
        word & kCMask,
    };
}

WordABx decode_abx(std::uint32_t word) {
    return WordABx {
        ((word >> kIShift) & kIMask) != 0U,
        (word >> kAShift) & kAMask,
        word & kBxMask,
    };
}

WordSAx decode_sax(std::uint32_t word) {
    const std::uint32_t raw = word & kAxMask;
    if ((raw & (1U << 24U)) == 0U) {
        return WordSAx {static_cast<std::int32_t>(raw)};
    }
    return WordSAx {static_cast<std::int32_t>(raw | (~kAxMask))};
}

std::string concat_operand_to_string(Value value) {
    switch (value.kind) {
        case ValueKind::String: return std::string(value.as_string("concat")->view());
        case ValueKind::Number: {
            std::ostringstream oss;
            oss << value.number_;
            return oss.str();
        }
        case ValueKind::Boolean: return value.bool_ ? "true" : "false";
        default: throw TypeError("concat: expected string/number/boolean");
    }
}

std::size_t to_array_length(Value value) {
    const double raw = value.as_number("newarray length");
    if (!std::isfinite(raw)) {
        throw TypeError("newarray length must be non-negative integer");
    }
    double integral = 0.0;
    if (std::modf(raw, &integral) != 0.0 || integral < 0.0) {
        throw TypeError("newarray length must be non-negative integer");
    }
    return static_cast<std::size_t>(integral);
}

std::size_t resolve_array_index(std::size_t len, Value value) {
    if (len > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        throw InternalError("array length exceeds int64 range");
    }
    const double raw = value.as_number("array index");
    if (!std::isfinite(raw)) {
        throw TypeError("array index must be integer");
    }
    double integral = 0.0;
    if (std::modf(raw, &integral) != 0.0) {
        throw TypeError("array index must be integer");
    }
    std::int64_t idx = static_cast<std::int64_t>(integral);
    if (idx < 0) {
        idx = static_cast<std::int64_t>(len) + idx;
    }
    if (idx < 0 || static_cast<std::size_t>(idx) >= len) {
        throw TypeError("array index out of bounds");
    }
    return static_cast<std::size_t>(idx);
}

} // namespace

Closure* VM::frame_closure() const {
    if (i_stack_.size() <= 1) {
        return nullptr; // sentinel has no function slot
    }
    return v_stack_.at(i_stack_.back().base).as_closure("current frame");
}

std::uint32_t VM::reg_limit() const {
    const CallFrame& frame = i_stack_.back();
    const Closure* closure = frame_closure();
    return checked_u32_stack_index(
        static_cast<std::size_t>(frame.base) + 1U
            + closure->code->chunks_[closure->chunk_index].slots, "register limit");
}

Value VM::reg_read(std::uint32_t index) const {
    const std::size_t at = static_cast<std::size_t>(i_stack_.back().base) + 1U + index;
    if (at >= reg_limit() || at >= v_stack_.size()) {
        throw InvalidCodeError("register index out of bounds");
    }
    return v_stack_[at];
}

void VM::reg_write(std::uint32_t index, Value value) {
    const std::size_t at = static_cast<std::size_t>(i_stack_.back().base) + 1U + index;
    if (at >= reg_limit() || at >= v_stack_.size()) {
        throw InvalidCodeError("register index out of bounds");
    }
    v_stack_[at] = value;
}

void VM::finish_frame_return(std::uint32_t result_begin, std::uint32_t result_end) {
    const CallFrame finished = i_stack_.back();
    if (result_begin > result_end || result_end > v_stack_.size()
        || result_begin < finished.base + 1U) {
        throw InvalidCodeError("invalid return result range");
    }
    const std::uint32_t available = result_end - result_begin;
    const std::uint32_t expected = finished.retc == multret ? available : finished.retc;
    const std::uint32_t emit = std::min(available, expected);
    const auto result_top = checked_u32_stack_index(
        static_cast<std::size_t>(finished.return_base) + expected, "return top");

    // The destination is below the callee's arguments. Forward copying is safe
    // even when a large result list overlaps its source. Grow before dropping
    // the frame so an allocation failure can still unwind it normally.
    v_stack_.resize(std::max(v_stack_.size(), static_cast<std::size_t>(result_top)), Value::nil());
    close_upvalues(finished.frame_start);
    i_stack_.pop_back();
    const Closure* caller = frame_closure();
    const bool bytecode_caller = caller != nullptr && caller->code != nullptr;
    // Keep the caller's physical register/old-prep regions, independently of top.
    const std::size_t retained = bytecode_caller ? finished.frame_start : finished.return_base;
    for (std::uint32_t i = 0; i < expected; ++i) {
        v_stack_[finished.return_base + i] = i < emit ? v_stack_[result_begin + i] : Value::nil();
    }
    v_stack_.resize(std::max(retained, static_cast<std::size_t>(result_top)));
    i_stack_.back().top = result_top;
}

void VM::make_call_frame(std::uint32_t arg_count, std::uint16_t retc,
                          std::uint32_t return_base) {
    if (retc > multret) {
        throw ApiError("return count out of bounds");
    }
    if (v_stack_.size() < static_cast<std::size_t>(arg_count) + 1U) {
        throw InvalidCodeError("call stack underflow");
    }
    const auto start = checked_u32_stack_index(
        v_stack_.size() - static_cast<std::size_t>(arg_count) - 1U, "frame start");
    const std::size_t caller_begin = i_stack_.size() == 1
        ? 0 : static_cast<std::size_t>(i_stack_.back().base) + 1U;
    if (start < caller_begin) {
        throw InvalidCodeError("call crosses caller frame boundary");
    }
    Closure* callee = v_stack_[start].as_closure("call");
    CallFrame frame {};
    frame.frame_start = frame.base = start;
    frame.top = checked_u32_stack_index(v_stack_.size(), "argument top");
    frame.return_base = return_base;
    frame.retc = retc;
    if (callee->code != nullptr) {
        if (callee->chunk_index >= callee->code->chunks_.size()) {
            throw InvalidImageError("callee chunk index out of bounds");
        }
        const Chunk& chunk = callee->code->chunks_[callee->chunk_index];
        if ((chunk.arity != 255 && chunk.arity > chunk.slots)
            || chunk.code_begin > chunk.code_end || chunk.code_end > callee->code->code_.size()) {
            throw InvalidImageError("invalid chunk bounds");
        }
        const std::uint32_t kept = chunk.arity == 255 ? arg_count : std::min(arg_count, std::uint32_t(chunk.arity));
        const std::uint32_t logical = chunk.arity == 255 ? arg_count : chunk.arity;
        frame.top = checked_u32_stack_index(static_cast<std::size_t>(start) + 1U + logical, "argument top");
        const auto size = checked_u32_stack_index(static_cast<std::size_t>(start) + 1U
            + std::max(logical, std::uint32_t(chunk.slots)), "frame extent");
        v_stack_.resize(static_cast<std::size_t>(start) + 1U + kept);
        v_stack_.resize(size, Value::nil());
        frame.pc = chunk.code_begin;
        frame.code_end = chunk.code_end;
    }
    i_stack_.push_back(frame);
}

void VM::run_c_frame() {
    Closure* current = frame_closure();
    if (current == nullptr || current->code != nullptr || current->cfunc == nullptr) {
        throw InternalError("invalid C frame");
    }
    const std::uint32_t reg_begin = i_stack_.back().base + 1U;
    const std::size_t produced_base = v_stack_.size();
    current->cfunc(this);
    const auto result_begin = v_stack_.size() < produced_base ? reg_begin : produced_base;
    finish_frame_return(
        checked_u32_stack_index(result_begin, "result begin"),
        checked_u32_stack_index(v_stack_.size(), "result end"));
}

void VM::run(std::size_t target_depth) {
    while (i_stack_.size() > target_depth) {
        CallFrame& frame = i_stack_.back();
        Closure* current = frame_closure();
        if (current == nullptr) {
            throw InternalError("invalid target depth");
        }

        if (current->code == nullptr) {
            run_c_frame();
            continue;
        }

        CodeUnit* cu = current->code;
        const std::uint32_t frame_limit = reg_limit();
        if (frame.pc >= frame.code_end) {
            throw InternalError("unexpected end of chunk");
        }
        if (static_cast<std::size_t>(frame.pc) >= cu->code_.size()) {
            throw InvalidCodeError("program counter out of bytecode bounds");
        }

        const std::uint32_t word = cu->code_[frame.pc++];
        const Op op = decode_op(word);

        switch (op) {
            case Op::Load: {
                const auto w = decode_abx(word);
                if (w.i) {
                    reg_write(w.a, Value::number(static_cast<double>(w.imm_bx())));
                    break;
                }
                reg_write(w.a, reg_read(w.bx));
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
                const auto w = decode_abx(word);
                if (w.bx >= cu->constants_.size()) {
                    throw InvalidCodeError("constant index out of bounds");
                }
                reg_write(w.a, cu->constants_[w.bx]);
                break;
            }
            case Op::GetGlobalK: {
                const auto w = decode_abx(word);
                if (w.a >= cu->constants_.size()) {
                    throw InvalidCodeError("global key constant index out of bounds");
                }
                const Value key = cu->constants_[w.a];
                if (key.kind != ValueKind::String) {
                    throw TableError("global key must be string");
                }
                Value out = Value::nil();
                if (!globals()->get(key, &out)) {
                    out = Value::nil();
                }
                reg_write(w.bx, out);
                break;
            }
            case Op::SetGlobalK: {
                const auto w = decode_abx(word);
                if (w.a >= cu->constants_.size()) {
                    throw InvalidCodeError("global key constant index out of bounds");
                }
                const Value key = cu->constants_[w.a];
                if (key.kind != ValueKind::String) {
                    throw TableError("global key must be string");
                }
                const Value value = w.i ? Value::number(static_cast<double>(w.imm_bx())) : reg_read(w.bx);
                if (!globals()->set(key, value)) {
                    throw TableError("failed to set global value");
                }
                break;
            }
            case Op::GetGlobal: {
                const auto w = decode_abx(word);
                Value out = Value::nil();
                if (!globals()->get(reg_read(w.a), &out)) {
                    out = Value::nil();
                }
                reg_write(w.bx, out);
                break;
            }
            case Op::SetGlobal: {
                const auto w = decode_abx(word);
                const Value value = w.i ? Value::number(static_cast<double>(w.imm_bx())) : reg_read(w.bx);
                if (!globals()->set(reg_read(w.a), value)) {
                    throw TableError("failed to set global value");
                }
                break;
            }
            case Op::Add: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    reg_read(w.b).as_number("binary op")
                    + (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                ));
                break;
            }
            case Op::Sub: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    reg_read(w.b).as_number("binary op")
                    - (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                ));
                break;
            }
            case Op::Mul: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    reg_read(w.b).as_number("binary op")
                    * (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                ));
                break;
            }
            case Op::Div: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    reg_read(w.b).as_number("binary op")
                    / (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                ));
                break;
            }
            case Op::Idiv: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    std::floor(
                        reg_read(w.b).as_number("binary op")
                        / (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                    )
                ));
                break;
            }
            case Op::Mod: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    std::fmod(
                        reg_read(w.b).as_number("binary op"),
                        (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                    )
                ));
                break;
            }
            case Op::Pow: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    std::pow(
                        reg_read(w.b).as_number("binary op"),
                        (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("binary op"))
                    )
                ));
                break;
            }
            case Op::Concat: {
                const auto w = decode_abc(word);
                const std::string lhs = concat_operand_to_string(reg_read(w.b));
                const std::string rhs = w.i
                    ? concat_operand_to_string(Value::number(static_cast<double>(w.imm_c())))
                    : concat_operand_to_string(reg_read(w.c));
                reg_write(w.a, Value::string(make_string(lhs + rhs)));
                break;
            }
            case Op::Eq: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    value_equals(
                        reg_read(w.b),
                        w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c)
                    )
                ));
                break;
            }
            case Op::Ne: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    !value_equals(
                        reg_read(w.b),
                        w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c)
                    )
                ));
                break;
            }
            case Op::Lt: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_number("compare")
                    < (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("compare"))
                ));
                break;
            }
            case Op::Le: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_number("compare")
                    <= (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("compare"))
                ));
                break;
            }
            case Op::Gt: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_number("compare")
                    > (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("compare"))
                ));
                break;
            }
            case Op::Ge: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_number("compare")
                    >= (w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("compare"))
                ));
                break;
            }
            case Op::And: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_boolean("logical op") && reg_read(w.c).as_boolean("logical op")
                ));
                break;
            }
            case Op::Or: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::boolean(
                    reg_read(w.b).as_boolean("logical op") || reg_read(w.c).as_boolean("logical op")
                ));
                break;
            }
            case Op::Band: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    static_cast<double>(
                        reg_read(w.b).as_integer("bit op")
                        & (w.i ? static_cast<std::int64_t>(w.imm_c()) : reg_read(w.c).as_integer("bit op"))
                    )
                ));
                break;
            }
            case Op::Bor: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    static_cast<double>(
                        reg_read(w.b).as_integer("bit op")
                        | (w.i ? static_cast<std::int64_t>(w.imm_c()) : reg_read(w.c).as_integer("bit op"))
                    )
                ));
                break;
            }
            case Op::Bxor: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    static_cast<double>(
                        reg_read(w.b).as_integer("bit op")
                        ^ (w.i ? static_cast<std::int64_t>(w.imm_c()) : reg_read(w.c).as_integer("bit op"))
                    )
                ));
                break;
            }
            case Op::Shl: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    static_cast<double>(
                        reg_read(w.b).as_integer("bit op")
                        << (w.i ? static_cast<std::int64_t>(w.imm_c()) : reg_read(w.c).as_integer("bit op"))
                    )
                ));
                break;
            }
            case Op::Shr: {
                const auto w = decode_abc(word);
                reg_write(w.a, Value::number(
                    static_cast<double>(
                        reg_read(w.b).as_integer("bit op")
                        >> (w.i ? static_cast<std::int64_t>(w.imm_c()) : reg_read(w.c).as_integer("bit op"))
                    )
                ));
                break;
            }
            case Op::Neg: {
                const auto w = decode_abx(word);
                reg_write(w.a, Value::number(-reg_read(w.bx).as_number("neg")));
                break;
            }
            case Op::Not: {
                const auto w = decode_abx(word);
                reg_write(w.a, Value::boolean(reg_read(w.bx).is_falsy()));
                break;
            }
            case Op::Len: {
                const auto w = decode_abx(word);
                const Value v = reg_read(w.bx);
                if (v.kind == ValueKind::String) {
                    const String* str = v.as_string("len");
                    reg_write(w.a, Value::number(static_cast<double>(str->len)));
                    break;
                }
                if (v.kind == ValueKind::Array) {
                    const Array* arr = v.as_array("len");
                    reg_write(w.a, Value::number(static_cast<double>(arr->elements.size())));
                    break;
                }
                throw TypeError("len: expected string/array");
                break;
            }
            case Op::NewTable: {
                const auto a = decode_abx(word).a;
                reg_write(a, Value::table(make_table()));
                break;
            }
            case Op::NewArray: {
                const auto w = decode_abx(word);
                if (w.i) {
                    reg_write(w.a, Value::array(make_array(to_array_length(Value::number(static_cast<double>(w.imm_bx()))))));
                    break;
                }
                reg_write(w.a, Value::array(make_array(to_array_length(reg_read(w.bx)))));
                break;
            }
            case Op::GetTable: {
                const auto w = decode_abc(word);
                Table* table = reg_read(w.b).as_table("gettable");
                Value out = Value::nil();
                if (!table->get(reg_read(w.c), &out)) {
                    out = Value::nil();
                }
                reg_write(w.a, out);
                break;
            }
            case Op::GetArray: {
                const auto w = decode_abc(word);
                Array* array = reg_read(w.b).as_array("getarray");
                const Value idx_v = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
                const std::size_t idx = resolve_array_index(array->elements.size(), idx_v);
                reg_write(w.a, array->elements[idx]);
                break;
            }
            case Op::SetTable: {
                const auto w = decode_abc(word);
                Table* table = reg_read(w.a).as_table("settable");
                const Value value = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
                if (!table->set(reg_read(w.b), value)) {
                    throw TableError("failed to set table key");
                }
                break;
            }
            case Op::SetArray: {
                const auto w = decode_abc(word);
                Array* array = reg_read(w.a).as_array("setarray");
                const std::size_t idx = resolve_array_index(array->elements.size(), reg_read(w.b));
                array->elements[idx] = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
                break;
            }
            case Op::GetArrayI: {
                const auto w = decode_abc(word);
                Array* array = reg_read(w.c).as_array("getarrayi");
                const Value idx_v = Value::number(static_cast<double>(w.imm_b()));
                const std::size_t idx = resolve_array_index(array->elements.size(), idx_v);
                reg_write(w.a, array->elements[idx]);
                break;
            }
            case Op::SetArrayI: {
                const auto w = decode_abc(word);
                Array* array = reg_read(w.a).as_array("setarrayi");
                const Value idx_v = Value::number(static_cast<double>(w.imm_b()));
                const std::size_t idx = resolve_array_index(array->elements.size(), idx_v);
                array->elements[idx] = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
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
                const auto w = decode_abc(word);
                const Value rhs = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
                if (!value_equals(reg_read(w.b), rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfNe: {
                const auto w = decode_abc(word);
                const Value rhs = w.i ? Value::number(static_cast<double>(w.imm_c())) : reg_read(w.c);
                if (value_equals(reg_read(w.b), rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfLt: {
                const auto w = decode_abc(word);
                const double rhs = w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("if compare");
                if (!(reg_read(w.b).as_number("if compare")
                    < rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfLe: {
                const auto w = decode_abc(word);
                const double rhs = w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("if compare");
                if (!(reg_read(w.b).as_number("if compare")
                    <= rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfGt: {
                const auto w = decode_abc(word);
                const double rhs = w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("if compare");
                if (!(reg_read(w.b).as_number("if compare")
                    > rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::IfGe: {
                const auto w = decode_abc(word);
                const double rhs = w.i ? static_cast<double>(w.imm_c()) : reg_read(w.c).as_number("if compare");
                if (!(reg_read(w.b).as_number("if compare")
                    >= rhs)) {
                    ++frame.pc;
                }
                break;
            }
            case Op::Call: {
                const auto w = decode_abc(word);
                const std::uint32_t reg_begin = frame.base + 1U;
                const std::uint32_t avail = frame_limit - reg_begin;
                if (w.a >= avail || (!w.i && w.a + 1U + w.b > avail)
                    || (w.c != multret && w.a + w.c > avail)) {
                    throw InvalidCodeError("call register range out of bounds");
                }
                const auto first_arg = reg_begin + w.a + 1U;
                if (w.i && (frame.top < first_arg || frame.top > v_stack_.size())) {
                    throw InvalidCodeError("invalid open argument range");
                }
                const auto argc = w.i ? frame.top - first_arg : w.b;
                const auto destination = reg_begin + w.a;
                const auto append_begin = checked_u32_stack_index(v_stack_.size(), "call frame start");
                checked_u32_stack_index(static_cast<std::size_t>(append_begin) + 1U + argc, "call extent");
                // Read by absolute index; vector reallocation must not invalidate sources.
                v_stack_.push_back(v_stack_[destination]);
                for (std::uint32_t i = 0; i < argc; ++i) {
                    v_stack_.push_back(v_stack_[first_arg + i]);
                }
                make_call_frame(argc, static_cast<std::uint16_t>(w.c), destination);
                break;
            }
            case Op::VargPrep: {
                const auto n = decode_abx(word).a;
                const auto old_base = frame.base;
                const auto reg_begin = old_base + 1U;
                if (n > frame_limit - reg_begin || frame.top < reg_begin || frame.top > v_stack_.size()) {
                    throw InvalidCodeError("invalid vargprep range");
                }
                const auto actual = frame.top - reg_begin;
                const auto extra = actual > n ? actual - n : 0U;
                const auto copied = std::min(actual, n);
                const Value function = v_stack_[old_base];
                // Never place a function slot over an old register: an open upvalue
                // may still refer to it after delayed or repeated preparation.
                const auto padded_top = checked_u32_stack_index(
                    static_cast<std::size_t>(reg_begin) + std::max(actual, n), "vargprep input");
                const auto extent = std::max(v_stack_.size(), static_cast<std::size_t>(padded_top));
                const bool reuse_extras = extent == padded_top;
                const auto new_base = checked_u32_stack_index(
                    extent + (reuse_extras ? 0U : extra), "vargprep base");
                const auto new_end = checked_u32_stack_index(
                    static_cast<std::size_t>(new_base) + 1U + (frame_limit - reg_begin), "vargprep extent");
                v_stack_.resize(new_end, Value::nil());
                if (!reuse_extras) {
                    for (std::uint32_t i = 0; i < extra; ++i) {
                        v_stack_[new_base - extra + i] = v_stack_[reg_begin + n + i];
                    }
                }
                v_stack_[new_base] = function;
                for (std::uint32_t i = 0; i < n; ++i) {
                    v_stack_[new_base + 1U + i] = i < copied ? v_stack_[reg_begin + i] : Value::nil();
                    v_stack_[reg_begin + i] = Value::nil();
                }
                frame.base = new_base;
                frame.nextra = extra;
                frame.top = new_base + 1U + n;
                break;
            }
            case Op::Varg: {
                const auto w = decode_abx(word);
                const auto reg_begin = frame.base + 1U;
                const auto slots = frame_limit - reg_begin;
                const auto count = w.i ? frame.nextra : w.bx;
                // An empty/open list may begin at the end of the fixed namespace.
                if (w.a > slots || (!w.i && w.bx > slots - w.a)) {
                    throw InvalidCodeError("varg destination out of bounds");
                }
                const auto destination = reg_begin + w.a;
                const auto end = checked_u32_stack_index(
                    static_cast<std::size_t>(destination) + count, "varg top");
                v_stack_.resize(std::max(v_stack_.size(), static_cast<std::size_t>(end)), Value::nil());
                for (std::uint32_t i = 0; i < count; ++i) {
                    v_stack_[destination + i] = i < frame.nextra
                        ? v_stack_[frame.base - frame.nextra + i] : Value::nil();
                }
                if (w.i) {
                    frame.top = end;
                }
                break;
            }
            case Op::Closure: {
                const auto w = decode_abx(word);
                if (w.bx >= cu->chunks_.size()) {
                    throw InvalidImageError("closure chunk index out of bounds");
                }
                const Chunk& chunk = cu->chunks_[w.bx];
                Closure* closure = make_closure(cu, w.bx);
                for (std::uint8_t i = 0; i < closure->len; ++i) {
                    const UpvalueInfo info = chunk.upvalue_infos[i];
                    if (info.source == UpvalueSource::Local) {
                        if (info.index >= frame_limit - (frame.base + 1U)) {
                            throw InvalidCodeError("upvalue capture register range out of bounds");
                        }
                        const std::uint32_t abs_slot = frame.base + 1U + info.index;
                        closure->at(i) = capture_upvalue(abs_slot);
                        continue;
                    }
                    if (info.source == UpvalueSource::Upvalue) {
                        if (info.index >= current->len) {
                            throw InvalidCodeError("upvalue capture index out of bounds");
                        }
                        Upvalue* captured = current->at(info.index);
                        if (captured == nullptr) {
                            throw InternalError("upvalue is not initialized");
                        }
                        closure->at(i) = captured;
                        continue;
                    }
                    throw InvalidImageError("unknown upvalue source");
                }
                reg_write(w.a, Value::closure(closure));
                break;
            }
            case Op::GetUpvalue: {
                const auto w = decode_abx(word);
                if (w.a > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(w.a) >= current->len) {
                    throw InvalidCodeError("upvalue index out of bounds");
                }
                Upvalue* upvalue = current->at(static_cast<std::uint8_t>(w.a));
                if (upvalue == nullptr) {
                    throw InternalError("upvalue is not initialized");
                }
                if (!upvalue->is_open) {
                    reg_write(w.bx, upvalue->closed);
                    break;
                }
                if (upvalue->slot >= v_stack_.size()) {
                    throw InternalError("open upvalue slot out of bounds");
                }
                reg_write(w.bx, v_stack_[upvalue->slot]);
                break;
            }
            case Op::SetUpvalue: {
                const auto w = decode_abx(word);
                if (w.a > std::numeric_limits<std::uint8_t>::max() || static_cast<std::uint8_t>(w.a) >= current->len) {
                    throw InvalidCodeError("upvalue index out of bounds");
                }
                Upvalue* upvalue = current->at(static_cast<std::uint8_t>(w.a));
                if (upvalue == nullptr) {
                    throw InternalError("upvalue is not initialized");
                }
                const Value value = w.i ? Value::number(static_cast<double>(w.imm_bx())) : reg_read(w.bx);
                if (!upvalue->is_open) {
                    upvalue->closed = value;
                    break;
                }
                if (upvalue->slot >= v_stack_.size()) {
                    throw InternalError("open upvalue slot out of bounds");
                }
                v_stack_[upvalue->slot] = value;
                break;
            }
            case Op::Return: {
                const auto w = decode_abx(word);
                const auto reg_begin = frame.base + 1U;
                const auto slots = frame_limit - reg_begin;
                if (w.a > slots || (!w.i && w.bx > slots - w.a)) {
                    throw InvalidCodeError("return register range out of bounds");
                }
                const auto result_begin = reg_begin + w.a;
                const auto result_end = w.i ? frame.top : result_begin + w.bx;
                finish_frame_return(result_begin, result_end);
                break;
            }
            default:
                throw InvalidCodeError("unknown opcode");
        }
    }
}

void VM::call(std::uint32_t arg_count, std::uint16_t retc) {
    if (i_stack_.empty()) {
        throw InternalError("call stack is not initialized");
    }

    const std::size_t caller_depth = i_stack_.size();
    if (stack_top() < static_cast<std::size_t>(arg_count) + 1U) {
        throw ApiError("call stack underflow");
    }
    const auto start = checked_u32_stack_index(v_stack_.size() - arg_count - 1U, "API call start");
    const auto saved_top = i_stack_.back().top;
    try {
        make_call_frame(arg_count, retc, start);
        run(caller_depth);
    } catch (...) {
        close_upvalues(start);
        i_stack_.resize(caller_depth);
        v_stack_.resize(start);
        i_stack_.back().top = std::min(saved_top, start);
        throw;
    }
}

} // namespace suru::vm

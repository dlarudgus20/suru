#pragma once

#include <cstdint>

namespace suru::vm {

enum class Op : std::uint8_t {
    Pop = 0x01,

    Nil = 0x02,
    True = 0x03,
    False = 0x04,
    Const = 0x05,

    GetLocal = 0x06,
    SetLocal = 0x07,
    GetGlobal = 0x08,
    SetGlobal = 0x09,

    Add = 0x10,
    Sub = 0x11,
    Mul = 0x12,
    Div = 0x13,
    Idiv = 0x14,
    Mod = 0x15,
    Pow = 0x16,
    Neg = 0x17,
    Not = 0x18,

    Eq = 0x20,
    Ne = 0x21,
    Lt = 0x22,
    Le = 0x23,
    Gt = 0x24,
    Ge = 0x25,

    Band = 0x30,
    Bor = 0x31,
    Bxor = 0x32,
    Shl = 0x33,
    Shr = 0x34,

    NewTable = 0x40,
    GetTable = 0x41,
    SetTable = 0x42,

    Jmp = 0x50,
    JmpIfFalse = 0x51,

    Call = 0x60,
    Return = 0x61,
    Closure = 0x62,
    GetUpvalue = 0x63,
    SetUpvalue = 0x64,
};

} // namespace suru::vm


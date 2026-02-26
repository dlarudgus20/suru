#pragma once

#include <cstdint>

namespace suru::vm {

enum class Op : std::uint8_t {
    Move = 0,

    LoadNil = 1,
    LoadTrue = 2,
    LoadFalse = 3,
    LoadK = 4,

    GetGlobal = 5,
    SetGlobal = 6,

    Add = 7,
    Sub = 8,
    Mul = 9,
    Div = 10,
    Idiv = 11,
    Mod = 12,
    Pow = 13,
    Neg = 14,
    Not = 15,
    And = 16,
    Or = 17,

    Eq = 18,
    Ne = 19,
    Lt = 20,
    Le = 21,
    Gt = 22,
    Ge = 23,

    Band = 24,
    Bor = 25,
    Bxor = 26,
    Shl = 27,
    Shr = 28,

    NewTable = 29,
    GetTable = 30,
    SetTable = 31,

    Jmp = 32,
    IfFalsey = 33,
    IfTruthy = 34,
    IfEq = 35,
    IfNe = 36,
    IfLt = 37,
    IfLe = 38,
    IfGt = 39,
    IfGe = 40,

    Call = 41,
    Return = 42,
    Closure = 43,
    GetUpvalue = 44,
    SetUpvalue = 45,
};

} // namespace suru::vm


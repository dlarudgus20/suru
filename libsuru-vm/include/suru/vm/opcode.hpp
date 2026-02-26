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
    Concat = 14,
    Neg = 15,
    Not = 16,
    Len = 17,
    And = 18,
    Or = 19,

    Eq = 20,
    Ne = 21,
    Lt = 22,
    Le = 23,
    Gt = 24,
    Ge = 25,

    Band = 26,
    Bor = 27,
    Bxor = 28,
    Shl = 29,
    Shr = 30,

    NewTable = 31,
    GetTable = 32,
    SetTable = 33,

    Jmp = 34,
    IfFalsy = 35,
    IfTruthy = 36,
    IfEq = 37,
    IfNe = 38,
    IfLt = 39,
    IfLe = 40,
    IfGt = 41,
    IfGe = 42,

    Call = 43,
    Return = 44,
    Closure = 45,
    GetUpvalue = 46,
    SetUpvalue = 47,
};

} // namespace suru::vm


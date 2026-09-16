#pragma once

#include <cstdint>

namespace suru::vm {

enum class Op : std::uint8_t {
    Load = 0,

    LoadNil = 1,
    LoadTrue = 2,
    LoadFalse = 3,
    LoadK = 4,

    GetGlobalK = 5,
    SetGlobalK = 6,

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
    NewArray = 34,
    GetArray = 35,
    SetArray = 36,

    Jmp = 37,
    IfFalsy = 38,
    IfTruthy = 39,
    IfEq = 40,
    IfNe = 41,
    IfLt = 42,
    IfLe = 43,
    IfGt = 44,
    IfGe = 45,

    Call = 46,
    Return = 47,
    Closure = 48,
    GetUpvalue = 49,
    SetUpvalue = 50,
    GetGlobal = 51,
    SetGlobal = 52,
    GetArrayI = 53,
    SetArrayI = 54,
    VargPrep = 55,
    Varg = 56,
    PushArrayX = 57,
    Close = 58,
};

} // namespace suru::vm

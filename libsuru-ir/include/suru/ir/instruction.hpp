#pragma once

#include <cstdint>

#include "suru/ir/opcode.hpp"

namespace suru::ir {

using Word = std::uint32_t;
inline constexpr std::uint16_t multret = 0x1ffU;

inline constexpr std::uint32_t op_shift = 26U;
inline constexpr std::uint32_t op_mask = 0x3FU;
inline constexpr std::uint32_t i_shift = 25U;
inline constexpr std::uint32_t i_mask = 0x1U;
inline constexpr std::uint32_t a_shift = 17U;
inline constexpr std::uint32_t a_mask = 0xFFU;
inline constexpr std::uint32_t b_shift = 9U;
inline constexpr std::uint32_t b_mask = 0xFFU;
inline constexpr std::uint32_t c_mask = 0x1FFU;
inline constexpr std::uint32_t bx_mask = 0x1FFFFU;
inline constexpr std::uint32_t ax_mask = 0x1FFFFFFU;

struct WordABC {
    bool i;
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;

    [[nodiscard]] constexpr std::int32_t imm_b() const {
        const std::uint32_t raw = b & b_mask;
        return (raw & (1U << 7U)) == 0U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw | ~b_mask);
    }

    [[nodiscard]] constexpr std::int32_t imm_c() const {
        const std::uint32_t raw = c & c_mask;
        return (raw & (1U << 8U)) == 0U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw | ~c_mask);
    }
};

struct WordABx {
    bool i;
    std::uint32_t a;
    std::uint32_t bx;

    [[nodiscard]] constexpr std::int32_t imm_bx() const {
        const std::uint32_t raw = bx & bx_mask;
        return (raw & (1U << 16U)) == 0U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw | ~bx_mask);
    }
};

struct WordSAx {
    std::int32_t sax;
};

[[nodiscard]] constexpr Op decode_op(Word word) {
    return static_cast<Op>((word >> op_shift) & op_mask);
}

[[nodiscard]] constexpr WordABC decode_abc(Word word) {
    return WordABC {
        ((word >> i_shift) & i_mask) != 0U,
        (word >> a_shift) & a_mask,
        (word >> b_shift) & b_mask,
        word & c_mask,
    };
}

[[nodiscard]] constexpr WordABx decode_abx(Word word) {
    return WordABx {
        ((word >> i_shift) & i_mask) != 0U,
        (word >> a_shift) & a_mask,
        word & bx_mask,
    };
}

[[nodiscard]] constexpr WordSAx decode_sax(Word word) {
    const std::uint32_t raw = word & ax_mask;
    return WordSAx {
        (raw & (1U << 24U)) == 0U
            ? static_cast<std::int32_t>(raw)
            : static_cast<std::int32_t>(raw | ~ax_mask)
    };
}

[[nodiscard]] constexpr Word encode_abc(
    Op op, std::uint32_t a, std::uint32_t b, std::uint32_t c, bool i = false
) {
    return (static_cast<std::uint32_t>(op) << op_shift)
        | ((i ? i_mask : 0U) << i_shift)
        | ((a & a_mask) << a_shift)
        | ((b & b_mask) << b_shift)
        | (c & c_mask);
}

[[nodiscard]] constexpr Word encode_abx(
    Op op, std::uint32_t a, std::uint32_t bx, bool i = false
) {
    return (static_cast<std::uint32_t>(op) << op_shift)
        | ((i ? i_mask : 0U) << i_shift)
        | ((a & a_mask) << a_shift)
        | (bx & bx_mask);
}

[[nodiscard]] constexpr Word encode_sax(Op op, std::int32_t sax) {
    return (static_cast<std::uint32_t>(op) << op_shift)
        | (static_cast<std::uint32_t>(sax) & ax_mask);
}

} // namespace suru::ir

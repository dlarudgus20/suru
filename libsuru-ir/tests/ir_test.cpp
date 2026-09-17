#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "suru/ir/assembler.hpp"
#include "suru/ir/builder.hpp"
#include "suru/ir/disassembler.hpp"
#include "suru/ir/error.hpp"
#include "suru/ir/image.hpp"
#include "suru/ir/instruction.hpp"

namespace {

constexpr std::string_view source = R"(.const
k0 = number 12.5
k1 = string "hello\nworld"
.entry main

.chunk child 1 2
.upvalue local 0
    GETUPVAL 0 0
    RETURN 0 1

.chunk main @va 8
    VARGPREP 1
    LOADK 1 k0
    NEWARRAY 2 #2
    SETINDEX 2 #0 1
    GETINDEX 3 2 #0
    IFEQ 3 #12
    JMP done
    CLOSURE 4 child
    CALL.v 4 @vret
done:
    PUSHARRAYX.v 2 4
    RETURN.v 3
)";

TEST(Ir, AssemblyDisassemblyRoundTrip) {
    const suru::ir::CodeUnit first = suru::ir::assemble(source);
    std::ostringstream text;
    suru::ir::disassemble(text, first);
    const suru::ir::CodeUnit second = suru::ir::assemble(text.str());

    ASSERT_EQ(second.constants.size(), first.constants.size());
    ASSERT_EQ(second.chunks.size(), first.chunks.size());
    ASSERT_EQ(second.entry_chunk, first.entry_chunk);
    for (std::size_t i = 0; i < first.chunks.size(); ++i) {
        EXPECT_EQ(second.chunks[i].name, first.chunks[i].name);
        EXPECT_EQ(second.chunks[i].arity, first.chunks[i].arity);
        EXPECT_EQ(second.chunks[i].slots, first.chunks[i].slots);
        EXPECT_EQ(second.chunks[i].code, first.chunks[i].code);
    }
}

TEST(Ir, BinaryRoundTripHasNoVersionField) {
    const suru::ir::CodeUnit first = suru::ir::assemble(source);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    suru::ir::write_binary(bytes, first);
    const std::string raw = bytes.str();
    ASSERT_GE(raw.size(), 8U);
    const auto first_header_field = static_cast<std::uint32_t>(static_cast<unsigned char>(raw[4]))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[5])) << 8U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[6])) << 16U)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[7])) << 24U);
    EXPECT_EQ(first_header_field, static_cast<std::uint32_t>(first.constants.size()));
    bytes.seekg(0);
    const suru::ir::CodeUnit second = suru::ir::read_binary(bytes);

    ASSERT_EQ(second.chunks.size(), first.chunks.size());
    EXPECT_EQ(second.chunks[1].code, first.chunks[1].code);
    EXPECT_EQ(second.entry_chunk, first.entry_chunk);
}

TEST(Ir, RejectsNumericMultretSentinel) {
    EXPECT_THROW(
        static_cast<void>(suru::ir::assemble(".chunk main 0 1\nCALL 0 0 511\n")),
        suru::ir::AssemblerError
    );
    EXPECT_THROW(
        static_cast<void>(suru::ir::assemble(".chunk main 0 1\nCALL 0 0 0x1ff\n")),
        suru::ir::AssemblerError
    );
}

TEST(Ir, AcceptsVretSpelling) {
    const auto unit = suru::ir::assemble(".chunk main 0 1\nCALL 0 0 @vret\n");
    ASSERT_EQ(unit.chunks.size(), 1U);
    EXPECT_EQ(suru::ir::decode_abc(unit.chunks[0].code[0]).c, suru::ir::multret);
}

TEST(Ir, ValidatesReferencesAndLogicalOperands) {
    auto invalid = suru::ir::assemble(".chunk main 0 1\nLOADK 0 99\n");
    EXPECT_THROW(suru::ir::validate(invalid), suru::ir::ImageError);
    EXPECT_THROW(
        static_cast<void>(suru::ir::assemble(".chunk main 0 1\nAND 0 0 #1\n")),
        suru::ir::AssemblerError
    );

    auto invalid_capture = suru::ir::assemble(R"(
.chunk main 0 1
CLOSURE 0 child
RETURN 0 0
.chunk child 0 1
.upvalue local 1
RETURN 0 0
)");
    EXPECT_THROW(suru::ir::validate(invalid_capture), suru::ir::ImageError);

    auto invalid_source = suru::ir::assemble(R"(
.chunk main 0 1
CLOSURE 0 child
RETURN 0 0
.chunk child 0 1
.upvalue local 0
RETURN 0 0
)");
    invalid_source.chunks[1].upvalue_infos[0].source =
        static_cast<suru::ir::UpvalueSource>(255);
    EXPECT_THROW(suru::ir::validate(invalid_source), suru::ir::ImageError);
}

TEST(Ir, AcceptsBothAssemblyCommentStyles) {
    auto unit = suru::ir::assemble(
        ".chunk main 0 0 ; header\nRETURN 0 0;\n"
    );
    ASSERT_EQ(unit.chunks.size(), 1U);
    EXPECT_EQ(unit.chunks[0].code.size(), 1U);
}


TEST(Ir, RequiresNonemptyUniqueChunkNamesAtEveryImageBoundary) {
    for (const std::string bad_name : {"", "main"}) {
        auto unit = suru::ir::assemble(".chunk main 0 0\nRETURN 0 0\n.chunk child 0 0\nRETURN 0 0\n");
        unit.chunks[1].name = bad_name;
        EXPECT_THROW(suru::ir::validate(unit), suru::ir::ImageError);
        std::ostringstream bytes;
        EXPECT_THROW(suru::ir::write_binary(bytes, unit), suru::ir::ImageError);
        EXPECT_TRUE(bytes.str().empty());
        std::ostringstream assembly;
        EXPECT_THROW(suru::ir::disassemble(assembly, unit), suru::ir::ImageError);
        suru::ir::CodeUnitBuilder builder;
        static_cast<void>(builder.add_chunk(unit.chunks[0]));
        static_cast<void>(builder.add_chunk(unit.chunks[1]));
        EXPECT_THROW(static_cast<void>(builder.finish(0)), suru::ir::ImageError);
    }
    // Invalid names also cannot enter through a hand-crafted SBC.
    auto unit = suru::ir::assemble(".chunk main 0 0\nRETURN 0 0\n.chunk next 0 0\nRETURN 0 0\n");
    std::ostringstream bytes;
    suru::ir::write_binary(bytes, unit);
    auto raw = bytes.str();
    const auto position = raw.find("next");
    ASSERT_NE(position, std::string::npos);
    raw.replace(position, 4, "main");
    std::istringstream invalid(raw);
    EXPECT_THROW(static_cast<void>(suru::ir::read_binary(invalid)), suru::ir::ImageError);
}

TEST(Ir, StoresUpvalueCountAsOneByteAndRejectsOverflow) {
    auto unit = suru::ir::assemble(".chunk main 0 0\nRETURN 0 0\n");
    unit.chunks[0].upvalue_infos.assign(255, {suru::ir::UpvalueSource::Local, 0});
    EXPECT_NO_THROW(suru::ir::validate(unit));
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    suru::ir::write_binary(bytes, unit);
    const auto raw = bytes.str();
    // Header 16, name length 4 + name 4, arity/slots/count 3,
    // captures 510, code count 4, one instruction 4.
    ASSERT_EQ(raw.size(), 545U);
    EXPECT_EQ(static_cast<unsigned char>(raw[26]), 255U);
    bytes.seekg(0);
    EXPECT_EQ(suru::ir::read_binary(bytes).chunks[0].upvalue_infos.size(), 255U);
    unit.chunks[0].upvalue_infos.push_back({suru::ir::UpvalueSource::Local, 0});
    EXPECT_THROW(suru::ir::validate(unit), suru::ir::ImageError);
    std::ostringstream rejected;
    EXPECT_THROW(suru::ir::write_binary(rejected, unit), suru::ir::ImageError);
    suru::ir::CodeUnitBuilder builder;
    static_cast<void>(builder.add_chunk(unit.chunks[0]));
    EXPECT_THROW(static_cast<void>(builder.finish(0)), suru::ir::ImageError);
}

} // namespace

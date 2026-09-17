#include <gtest/gtest.h>

#include <string_view>
#include <string>
#include <sstream>
#include "suru/front/semantic.hpp"
#include "suru/ir/assembler.hpp"
#include "suru/ir/disassembler.hpp"
#include "suru/ir/image.hpp"

#include "suru/front/compile.hpp"
#include "suru/front/parse.hpp"
#include "suru/vm/raised_error.hpp"
#include "suru/vm/vm.hpp"

namespace {

suru::vm::Value execute(std::string_view source, std::string_view global = "result") {
    auto parsed = suru::front::parse(source, "<test>");
    EXPECT_TRUE(parsed.ok());
    if (!parsed.ok()) return suru::vm::Value::nil();
    auto compiled = suru::front::compile(parsed.tree);
    EXPECT_TRUE(compiled.ok());
    if (!compiled.ok()) return suru::vm::Value::nil();

    suru::vm::VM vm;
    auto* closure = vm.load_code_unit(compiled.code);
    vm.push_value(suru::vm::Value::closure(closure));
    vm.call(0, 0);
    suru::vm::Value value;
    EXPECT_TRUE(vm.globals()->get(suru::vm::Value::string(vm.make_string(global)), &value));
    return value;
}

TEST(Compiler, ResolvesNestedUpvaluesAndRecursion) {
    auto parsed = suru::front::parse(R"(
local fn outer(x)
    local fn inner(y) return x + y end
    return inner
end
local f = outer(40)
result = f(2)
)");
    ASSERT_TRUE(parsed.ok());
    auto compiled = suru::front::compile(parsed.tree);
    ASSERT_TRUE(compiled.ok());
    ASSERT_EQ(compiled.code.chunks.size(), 3U);
    EXPECT_FALSE(compiled.code.chunks[2].upvalue_infos.empty());

    const auto value = execute(R"(
local fn outer(x)
    local fn inner(y) return x + y end
    return inner
end
local f = outer(40)
result = f(2)
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 42);
}

TEST(Compiler, ImplementsMultipleValuesVarargsAndArrayTail) {
    const auto value = execute(R"(
local fn values(a, ...) return a, ... end
local array = [10, values(20, 30, 40)]
local a, b, c = values(array[0], array[2], array[-1])
result = a + b + c + #array
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 84);
}

TEST(Compiler, ImplementsMethodsLoopsAndLabeledControl) {
    const auto value = execute(R"(
local object = {value = 1}
local receiver_calls = 0
local fn receiver()
    receiver_calls = receiver_calls + 1
    return object
end
fn object:add(x) self.value = self.value + x return self.value end
local sum = 0
outer: for i = 0, 8 do
    if i == 2 then continue outer end
    sum = sum + receiver():add(i)
    if i == 5 then break outer end
end
result = sum * 10 + receiver_calls
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 315);
}

TEST(Compiler, LocalInitializerSeesOuterBindingAndAssignmentIsSimultaneous) {
    const auto value = execute(R"(
local x = 10
local y = 2
do
    local x = x + 1
    x, y = y, x
    result = x * 100 + y
end
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 211);
}

TEST(Compiler, EvaluatesRhsBeforeComputedAssignmentTargets) {
    const auto value = execute(R"(
local table = {}
local sequence = 0
local fn key() sequence = sequence + 1 return sequence end
local fn rhs() sequence = sequence + 10 return 99 end
table[key()] = rhs()
result = sequence * 100 + table[11]
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 1199);
}

TEST(Compiler, CloseSeparatesCapturedLoopIterations) {
    const auto value = execute(R"(
local f0
local f1
for i = 0, 2 do
    if i == 0 then
        f0 = fn() return i end
    else
        f1 = fn() return i end
    end
end
result = f0() * 10 + f1()
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 1);
}

TEST(Compiler, ImplementsGenericForAndRepeatContinue) {
    const auto value = execute(R"(
local fn iter(limit, control)
    local next = control + 1
    if next < limit then return next, next * 2 end
    return nil
end
local sum = 0
local f0
local f1
for key, value in iter, 4, -1 do
    sum = sum + value
    if key == 0 then f0 = fn() return key end end
    if key == 1 then f1 = fn() return key end end
end
local i = 0
repeat
    i = i + 1
    if i < 3 then continue end
until i >= 3
result = (sum + i) * 10 + f0() * 2 + f1()
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 151);
}

TEST(Compiler, RejectsInvalidVarargLoopLabelAndConstantZeroStep) {
    for (const std::string_view source : {
        "return ...",
        "break missing",
        "for i = 0, 2, 0 do end",
    }) {
        auto parsed = suru::front::parse(source);
        ASSERT_TRUE(parsed.ok()) << source;
        EXPECT_FALSE(suru::front::compile(parsed.tree).ok()) << source;
    }
}

TEST(Compiler, RaisesDynamicZeroNumericForStep) {
    EXPECT_THROW(
        static_cast<void>(execute(R"(
local step = 0
for i = 0, 2, step do end
result = 1
)")),
        suru::vm::RaisedError
    );
}


TEST(Compiler, UsesParentPrototypeNamesAndRoundTripsClosures) {
    auto parsed = suru::front::parse(R"(
local fn outer()
    local fn first() end
    local fn second()
        local fn inner() end
    end
end
local fn outer() end
do local fn outer() end end
local a = fn() return fn() end end
local b = fn() end
)");
    ASSERT_TRUE(parsed.ok());
    auto compiled = suru::front::compile(parsed.tree);
    ASSERT_TRUE(compiled.ok());
    const std::vector<std::string> names {
        "<main>", "outer@0", "outer@0::first@0", "outer@0::second@1",
        "outer@0::second@1::inner@0", "outer@1", "outer@2",
        "lambda@3", "lambda@3::lambda@0", "lambda@4"
    };
    ASSERT_EQ(compiled.code.chunks.size(), names.size());
    std::ostringstream assembly;
    suru::ir::disassemble(assembly, compiled.code);
    const auto roundtrip = suru::ir::assemble(assembly.str());
    EXPECT_EQ(roundtrip.entry_chunk, compiled.code.entry_chunk);
    ASSERT_EQ(roundtrip.chunks.size(), names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        EXPECT_EQ(compiled.code.chunks[i].name, names[i]);
        EXPECT_EQ(roundtrip.chunks[i].name, names[i]);
        EXPECT_EQ(roundtrip.chunks[i].code, compiled.code.chunks[i].code);
    }
}

TEST(Compiler, RejectsLoopControlAcrossFunctionBoundaries) {
    for (const std::string jump : {"break", "continue", "break outer", "continue outer"}) {
        for (const bool deep : {false, true}) {
            const std::string source = "outer: while true do local fn f() "
                + std::string(deep ? "local fn g() " : "") + jump
                + (deep ? " end" : "") + " end end";
            auto parsed = suru::front::parse(source);
            ASSERT_TRUE(parsed.ok()) << source;
            auto resolved = suru::front::resolve(parsed.tree);
            ASSERT_FALSE(resolved.ok()) << source;
            ASSERT_FALSE(resolved.diagnostics.empty());
            EXPECT_EQ(resolved.diagnostics.front().message,
                jump.find(' ') == std::string::npos
                    ? "loop control outside loop" : "unknown loop label: outer");
        }
    }
}

TEST(Compiler, NestedFunctionOwnsLoopsAndCloseBoundaries) {
    const auto value = execute(R"(
local saved
outer: for i = 0, 1 do
    local fn f()
        local j = 0
        inner: repeat
            local x = j
            saved = fn() return x end
            j = j + 1
            if j < 2 then continue inner end
            break inner
        until false
        return saved()
    end
    result = f()
    break outer
end
)");
    ASSERT_EQ(value.kind, suru::vm::ValueKind::Number);
    EXPECT_EQ(value.number_, 1);
}

TEST(Compiler, BoundsUpvaluesBeforeIndexNarrowing) {
    for (const std::size_t count : {255U, 256U}) {
        // Spread captured locals over parents so the local-register limit does
        // not mask the upvalue boundary in the innermost function.
        std::string source = "local a = 1 local fn parent() ";
        for (std::size_t i = 1; i < count; ++i) source += "local x" + std::to_string(i) + " = 1 ";
        source += "return fn() sink(a) ";
        for (std::size_t i = 1; i < count; ++i) source += "sink(x" + std::to_string(i) + ") ";
        source += "end end";
        auto parsed = suru::front::parse(source);
        ASSERT_TRUE(parsed.ok());
        auto resolved = suru::front::resolve(parsed.tree);
        if (count == 255) {
            ASSERT_TRUE(resolved.ok());
            auto compiled = suru::front::compile(parsed.tree, resolved.model);
            ASSERT_TRUE(compiled.ok());
            EXPECT_EQ(compiled.code.chunks.back().upvalue_infos.size(), 255U);
        } else {
            ASSERT_FALSE(resolved.ok());
            bool found = false;
            for (const auto& diagnostic : resolved.diagnostics) {
                if (diagnostic.message == "function captures more than 255 upvalues") found = true;
            }
            EXPECT_TRUE(found);
        }
    }
}

} // namespace

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/value.hpp"
#include "suru/vm/object.hpp"
#include "suru/vm/codeunit.hpp"
#include "suru/vm/stringset.hpp"
#include "suru/ir/codeunit.hpp"

namespace suru::vm {

class VM {
public:
    VM();
    ~VM();

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;
    VM(VM&&) = delete;
    VM& operator=(VM&&) = delete;

    static constexpr std::uint16_t multret = 0x1ff;
    void call(std::uint32_t arg_count, std::uint16_t retc);
    [[noreturn]] void raise(Value payload);

    [[nodiscard]] String* make_string(std::string_view text);
    [[nodiscard]] Array* make_array(std::size_t len);
    [[nodiscard]] Table* make_table();
    [[nodiscard]] Closure* make_closure_c(CFunction func, std::uint8_t n);

    [[nodiscard]] CodeUnit* make_code_unit();
    [[nodiscard]] Closure* make_closure(CodeUnit* cu, std::uint32_t chunk_index);
    [[nodiscard]] Closure* load_code_unit(const suru::ir::CodeUnit& image);

    [[nodiscard]] Table* globals();

    [[nodiscard]] Value pop_value();
    void push_value(Value value);

    [[nodiscard]] std::size_t stack_top() const;
    [[nodiscard]] Value getlocal(std::uint32_t index) const;
    [[nodiscard]] Value getupvalue(std::uint8_t index) const;

private:
    struct CallFrame {
        std::uint32_t pc {0};
        std::uint32_t frame_start {0};
        std::uint32_t base {0};
        std::uint32_t top {0};
        std::uint32_t nextra {0};
        std::uint32_t code_end {0};
        std::uint32_t return_base {0};
        std::uint16_t retc {0};
    };

    template <typename T>
    T* allocate_object(size_t size, size_t align);

    Upvalue* make_upvalue();
    Upvalue* capture_upvalue(std::uint32_t abs_slot);
    void close_upvalues(std::uint32_t from_base);

    Object* objects_ {nullptr};
    Table* global_table_ {nullptr};
    StringSet interned_strings_;

    std::vector<std::unique_ptr<CodeUnit>> code_units_;

    std::vector<Value> v_stack_;
    std::vector<CallFrame> i_stack_;
    Upvalue* open_upvalues_ {nullptr};

    [[nodiscard]] Closure* frame_closure() const;
    [[nodiscard]] std::uint32_t reg_limit() const;
    void make_call_frame(std::uint32_t arg_count, std::uint16_t retc, std::uint32_t return_base);
    void finish_frame_return(
        std::uint32_t result_begin,
        std::uint32_t result_end
    );
    void run_c_frame();
    void run(std::size_t target_depth);
    [[nodiscard]] Value reg_read(std::uint32_t index) const;
    void reg_write(std::uint32_t index, Value value);
};

} // namespace suru::vm

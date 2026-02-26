#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "suru/vm/value.hpp"
#include "suru/vm/object.hpp"
#include "suru/vm/codeunit.hpp"
#include "suru/vm/stringset.hpp"

namespace suru::vm {

class VM {
public:
    VM();
    ~VM();

    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;
    VM(VM&&) = delete;
    VM& operator=(VM&&) = delete;

    void call(std::size_t arg_count, std::size_t ret_slots);

    String* make_string(std::string_view text);
    Table* make_table();
    Closure* make_closure_c(CFunction func, size_t n);

    CodeUnit* make_code_unit();
    Closure* make_closure(CodeUnit* cu, size_t chunk_index, size_t n);

    Table* globals();

    [[nodiscard]] Value pop_value();
    void push_value(Value value);

    [[nodiscard]] std::size_t stack_top() const;
    [[nodiscard]] Value getlocal(std::size_t index) const;
    [[nodiscard]] Value getupvalue(std::size_t index) const;

private:
    struct CallFrame {
        Closure* closure {nullptr};
        std::size_t pc {0};
        std::size_t base {0};
        std::size_t code_end {0};
        std::size_t ret_slots {0};
    };

    template <typename T>
    T* allocate_object(size_t size, size_t align);

    Object* objects_ {nullptr};
    Table* global_table_ {nullptr};
    StringSet interned_strings_;

    std::vector<std::unique_ptr<CodeUnit>> code_units_;

    std::vector<Value> v_stack_;
    std::vector<CallFrame> i_stack_;

    void make_call_frame(std::size_t arg_count, std::size_t ret_slots);
    void run_c_frame();
    void run(std::size_t target_depth);
};

} // namespace suru::vm


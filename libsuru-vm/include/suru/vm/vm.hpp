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

    void exec_call();

    String* load_string(std::string_view text);
    Table* load_table();
    Closure* load_closure_c(CFunction func, size_t n);

    CodeUnit* load_code_unit();
    Closure* load_closure(CodeUnit* cu, size_t chunk_index, size_t n);

    Table* globals();
    const Table* globals() const;

    [[nodiscard]] Value pop_value();
    void push_value(Value value);

    [[nodiscard]] std::size_t c_arg_count() const;
    [[nodiscard]] Value c_arg(std::size_t index) const;

private:
    struct CallFrame {
        Closure* closure {nullptr};
        std::size_t pc {0};
        std::size_t base {0};
        std::size_t code_end {0};
        std::size_t expected_results {0};
    };

    template <typename T>
    T* allocate_object(size_t size, size_t align);

    Object* objects_ {nullptr};
    Table* global_table_ {nullptr};
    StringSet interned_strings_;

    std::vector<std::unique_ptr<CodeUnit>> code_units_;

    std::vector<Value> v_stack_;
    std::vector<CallFrame> i_stack_;

    bool c_call_active_ {false};
    std::size_t c_arg_base_ {0};
    std::size_t c_arg_count_ {0};
};

} // namespace suru::vm


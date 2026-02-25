#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

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

private:
    template <typename T>
    T* allocate_object(size_t size, size_t align);

    Object* objects_ {nullptr};
    Table* global_table_ {nullptr};
    StringSet interned_strings_;

    std::vector<std::unique_ptr<CodeUnit>> code_units_;

    std::vector<Value> v_stack_;
    std::vector<std::pair<Closure*, size_t>> i_stack_;
};

} // namespace suru::vm


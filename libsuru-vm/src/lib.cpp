#include "suru/lib/lib.hpp"

#include <cmath>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

#include "suru/vm/error.hpp"

namespace suru::lib {
namespace {

std::string value_to_string(suru::vm::Value value) {
    using suru::vm::ValueKind;
    switch (value.kind) {
        case ValueKind::Nil: return "nil";
        case ValueKind::Boolean: return value.bool_ ? "true" : "false";
        case ValueKind::Number: return std::to_string(value.number_);
        case ValueKind::String: return value.string_ != nullptr ? std::string(value.string_->view()) : "<null-string>";
        case ValueKind::Table: return "<table>";
        case ValueKind::Closure: return "<function>";
        default: return "<unknown>";
    }
}

bool is_integer_number(double value) {
    return std::floor(value) == value;
}

std::size_t table_max_array_index(const suru::vm::Table* table) {
    bool found = false;
    std::size_t max_index = 0;
    for (const auto& [key, value] : table->entries) {
        (void)value;
        if (key.kind != suru::vm::ValueKind::Number) {
            continue;
        }
        if (key.number_ < 0.0 || !is_integer_number(key.number_)) {
            continue;
        }
        const std::size_t idx = static_cast<std::size_t>(key.number_);
        if (!found || idx > max_index) {
            found = true;
            max_index = idx;
        }
    }
    if (!found) {
        return static_cast<std::size_t>(-1);
    }
    return max_index;
}

suru::vm::Value std_nil() {
    return suru::vm::Value::nil();
}

void std_print(suru::vm::VM* vm) {
    for (std::size_t i = 0; i < vm->stack_top(); ++i) {
        if (i != 0) {
            std::cout << '\t';
        }
        std::cout << value_to_string(vm->getlocal(i));
    }
    std::cout << '\n';
}

void std_read_line(suru::vm::VM* vm) {
    if (vm->stack_top() >= 1U) {
        suru::vm::Value prompt = vm->getlocal(0);
        if (prompt.kind != suru::vm::ValueKind::String || prompt.string_ == nullptr) {
            vm->push_value(std_nil());
            return;
        }
        std::cout << prompt.string_->view();
        std::cout.flush();
    }

    std::string line;
    if (!std::getline(std::cin, line)) {
        vm->push_value(std_nil());
        return;
    }
    vm->push_value(suru::vm::Value::string(vm->make_string(line)));
}

void std_to_number(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    suru::vm::Value arg = vm->getlocal(0);
    if (arg.kind == suru::vm::ValueKind::Number) {
        vm->push_value(arg);
        return;
    }
    if (arg.kind != suru::vm::ValueKind::String || arg.string_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    std::size_t pos = 0;
    try {
        const std::string text(arg.string_->view());
        double parsed = std::stod(text, &pos);
        if (pos != text.size()) {
            vm->push_value(std_nil());
            return;
        }
        vm->push_value(suru::vm::Value::number(parsed));
    } catch (...) {
        vm->push_value(std_nil());
    }
}

void std_to_string(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value arg = vm->getlocal(0);
    if (arg.kind == suru::vm::ValueKind::String) {
        vm->push_value(arg);
        return;
    }

    vm->push_value(suru::vm::Value::string(vm->make_string(value_to_string(arg))));
}

void std_type(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value arg = vm->getlocal(0);
    std::string_view name = "nil";
    switch (arg.kind) {
        case suru::vm::ValueKind::Nil: name = "nil"; break;
        case suru::vm::ValueKind::Boolean: name = "boolean"; break;
        case suru::vm::ValueKind::Number: name = "number"; break;
        case suru::vm::ValueKind::String: name = "string"; break;
        case suru::vm::ValueKind::Table: name = "table"; break;
        case suru::vm::ValueKind::Closure: name = "function"; break;
        default: name = "unknown"; break;
    }

    vm->push_value(suru::vm::Value::string(vm->make_string(name)));
}

void std_div(suru::vm::VM* vm) {
    if (vm->stack_top() < 2U) {
        vm->push_value(std_nil());
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value lhs = vm->getlocal(0);
    const suru::vm::Value rhs = vm->getlocal(1);
    if (lhs.kind != suru::vm::ValueKind::Number || rhs.kind != suru::vm::ValueKind::Number || rhs.number_ == 0.0) {
        vm->push_value(std_nil());
        vm->push_value(std_nil());
        return;
    }

    const double quotient = std::floor(lhs.number_ / rhs.number_);
    const double remainder = std::fmod(lhs.number_, rhs.number_);
    vm->push_value(suru::vm::Value::number(quotient));
    vm->push_value(suru::vm::Value::number(remainder));
}

void std_measure(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value target = vm->getlocal(0);
    if (target.kind != suru::vm::ValueKind::Closure || target.closure_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    vm->push_value(suru::vm::Value::closure(target.closure_));
    const auto begin = std::chrono::steady_clock::now();
    vm->call(0, 0);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - begin;
    vm->push_value(suru::vm::Value::number(elapsed.count()));
}

void str_trim(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }
    const suru::vm::Value arg = vm->getlocal(0);
    if (arg.kind != suru::vm::ValueKind::String || arg.string_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    std::string_view sv = arg.string_->view();
    std::size_t begin = 0;
    while (begin < sv.size() && std::isspace(static_cast<unsigned char>(sv[begin])) != 0) {
        ++begin;
    }
    std::size_t end = sv.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(sv[end - 1])) != 0) {
        --end;
    }

    vm->push_value(suru::vm::Value::string(vm->make_string(sv.substr(begin, end - begin))));
}

void str_split(suru::vm::VM* vm) {
    if (vm->stack_top() < 2U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value s = vm->getlocal(0);
    const suru::vm::Value sep = vm->getlocal(1);
    if (s.kind != suru::vm::ValueKind::String || s.string_ == nullptr ||
        sep.kind != suru::vm::ValueKind::String || sep.string_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    bool keep_empty = false;
    if (vm->stack_top() >= 3U) {
        const suru::vm::Value opt = vm->getlocal(2);
        if (opt.kind != suru::vm::ValueKind::Boolean) {
            vm->push_value(std_nil());
            return;
        }
        keep_empty = opt.bool_;
    }

    const std::string_view text = s.string_->view();
    const std::string_view delim = sep.string_->view();
    if (delim.empty()) {
        vm->push_value(std_nil());
        return;
    }

    suru::vm::Table* out = vm->make_table();
    std::size_t out_index = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t found = text.find(delim, pos);
        const std::size_t end = (found == std::string_view::npos) ? text.size() : found;
        const std::string_view token = text.substr(pos, end - pos);
        if (keep_empty || !token.empty()) {
            out->set(
                suru::vm::Value::number(static_cast<double>(out_index++)),
                suru::vm::Value::string(vm->make_string(token))
            );
        }

        if (found == std::string_view::npos) {
            break;
        }
        pos = found + delim.size();
    }

    vm->push_value(suru::vm::Value::table(out));
}

void table_keys(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value arg = vm->getlocal(0);
    if (arg.kind != suru::vm::ValueKind::Table || arg.table_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    suru::vm::Table* out = vm->make_table();
    std::size_t idx = 0;
    for (const auto& [k, v] : arg.table_->entries) {
        (void)v;
        out->set(suru::vm::Value::number(static_cast<double>(idx++)), k);
    }
    vm->push_value(suru::vm::Value::table(out));
}

void table_push(suru::vm::VM* vm) {
    if (vm->stack_top() < 2U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value table_v = vm->getlocal(0);
    if (table_v.kind != suru::vm::ValueKind::Table || table_v.table_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    const std::size_t max_index = table_max_array_index(table_v.table_);
    const double next_index = (max_index == static_cast<std::size_t>(-1)) ? 0.0 : static_cast<double>(max_index + 1U);
    if (!table_v.table_->set(suru::vm::Value::number(next_index), vm->getlocal(1))) {
        vm->push_value(std_nil());
        return;
    }

    vm->push_value(suru::vm::Value::number(next_index + 1.0));
}

void table_pop(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value table_v = vm->getlocal(0);
    if (table_v.kind != suru::vm::ValueKind::Table || table_v.table_ == nullptr) {
        vm->push_value(std_nil());
        return;
    }

    const std::size_t max_index = table_max_array_index(table_v.table_);
    if (max_index == static_cast<std::size_t>(-1)) {
        vm->push_value(std_nil());
        return;
    }

    const suru::vm::Value key = suru::vm::Value::number(static_cast<double>(max_index));
    suru::vm::Value out = suru::vm::Value::nil();
    if (!table_v.table_->get(key, &out)) {
        vm->push_value(std_nil());
        return;
    }
    (void)table_v.table_->erase(key);
    vm->push_value(out);
}

void math_abs(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U || vm->getlocal(0).kind != suru::vm::ValueKind::Number) {
        vm->push_value(std_nil());
        return;
    }
    vm->push_value(suru::vm::Value::number(std::abs(vm->getlocal(0).number_)));
}

void math_floor(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U || vm->getlocal(0).kind != suru::vm::ValueKind::Number) {
        vm->push_value(std_nil());
        return;
    }
    vm->push_value(suru::vm::Value::number(std::floor(vm->getlocal(0).number_)));
}

void math_ceil(suru::vm::VM* vm) {
    if (vm->stack_top() < 1U || vm->getlocal(0).kind != suru::vm::ValueKind::Number) {
        vm->push_value(std_nil());
        return;
    }
    vm->push_value(suru::vm::Value::number(std::ceil(vm->getlocal(0).number_)));
}

void register_cfunc(suru::vm::VM& vm, std::string_view name, suru::vm::CFunction fn) {
    suru::vm::String* key = vm.make_string(name);
    suru::vm::Closure* func = vm.make_closure_c(fn, 0);
    if (key == nullptr || func == nullptr) {
        throw suru::vm::InternalError("failed to create standard function");
    }

    if (!vm.globals()->set(suru::vm::Value::string(key), suru::vm::Value::closure(func))) {
        throw suru::vm::InternalError("failed to register standard function");
    }
}

void register_module_func(
    suru::vm::VM& vm,
    std::string_view module_name,
    std::string_view function_name,
    suru::vm::CFunction fn
) {
    suru::vm::String* module_key = vm.make_string(module_name);
    suru::vm::Value module_value = suru::vm::Value::nil();
    if (!vm.globals()->get(suru::vm::Value::string(module_key), &module_value)) {
        suru::vm::Table* new_module = vm.make_table();
        if (!vm.globals()->set(suru::vm::Value::string(module_key), suru::vm::Value::table(new_module))) {
            throw suru::vm::InternalError("failed to create module table");
        }
        module_value = suru::vm::Value::table(new_module);
    }

    if (module_value.kind != suru::vm::ValueKind::Table || module_value.table_ == nullptr) {
        throw suru::vm::TypeError("module value is not table");
    }

    suru::vm::String* fn_key = vm.make_string(function_name);
    suru::vm::Closure* fn_value = vm.make_closure_c(fn, 0);
    if (!module_value.table_->set(suru::vm::Value::string(fn_key), suru::vm::Value::closure(fn_value))) {
        throw suru::vm::InternalError("failed to register module function");
    }
}

} // namespace

void load_libs(suru::vm::VM& vm) {
    register_cfunc(vm, "print", &std_print);
    register_cfunc(vm, "read_line", &std_read_line);
    register_cfunc(vm, "to_number", &std_to_number);
    register_cfunc(vm, "to_string", &std_to_string);
    register_cfunc(vm, "type", &std_type);
    register_cfunc(vm, "div", &std_div);
    register_cfunc(vm, "measure", &std_measure);

    register_module_func(vm, "str", "trim", &str_trim);
    register_module_func(vm, "str", "split", &str_split);

    register_module_func(vm, "table", "keys", &table_keys);
    register_module_func(vm, "table", "push", &table_push);
    register_module_func(vm, "table", "pop", &table_pop);

    register_module_func(vm, "math", "abs", &math_abs);
    register_module_func(vm, "math", "floor", &math_floor);
    register_module_func(vm, "math", "ceil", &math_ceil);
}

} // namespace suru::lib

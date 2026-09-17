#include "suru/front/compile.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "suru/ir/instruction.hpp"

namespace suru::front {
namespace {

enum class ResultKind { One, Fixed, Open, Discard };
struct ResultMode {
    ResultKind kind {ResultKind::One};
    std::uint16_t count {1};
};

bool multi_valued(const Expr& expr) {
    return std::holds_alternative<CallExpr>(expr.kind)
        || std::holds_alternative<MethodCallExpr>(expr.kind)
        || std::holds_alternative<VarargExpr>(expr.kind);
}

class Compiler;

class FunctionCompiler {
public:
    FunctionCompiler(Compiler& owner, NodeId body, std::string name);
    suru::ir::Chunk compile(const Block& block, std::uint8_t arity, bool vararg);

private:
    struct LoopEmit {
        NodeId statement {0};
        std::vector<std::size_t> breaks;
        std::vector<std::size_t> continues;
    };
    struct LValue {
        enum class Kind { Binding, Index } kind {Kind::Binding};
        ResolvedBinding binding;
        std::uint8_t object {0};
        std::uint8_t key {0};
    };

    std::uint8_t allocate(std::uint16_t count = 1);
    void ensure(std::uint16_t end);
    void reset(std::uint16_t top) { temp_top_ = top; }
    std::size_t emit(suru::ir::Word word);
    std::size_t jump();
    void patch(std::size_t at, std::size_t target);
    std::size_t jump_if_falsy(std::uint8_t reg);
    std::size_t jump_if_truthy(std::uint8_t reg);
    void load_nil(std::uint8_t reg);
    void load_number(std::uint8_t reg, double value);
    void load_string(std::uint8_t reg, std::string_view value);
    void read_binding(const ResolvedBinding& binding, std::uint8_t destination);
    void write_binding(const ResolvedBinding& binding, std::uint8_t source);
    const ResolvedBinding& binding(NodeId id) const;
    const std::vector<std::uint8_t>& declarations(NodeId id) const;
    std::uint8_t block_base(NodeId id) const;

    void expression(const Expr& expr, std::uint8_t destination, ResultMode mode = {});
    void regular_tail(std::uint8_t destination, ResultMode mode);
    void call(const Expr& expr, const CallExpr& node, std::uint8_t destination, ResultMode mode);
    void method_call(const Expr& expr, const MethodCallExpr& node, std::uint8_t destination, ResultMode mode);
    bool expression_list_open(const std::vector<ExprPtr>& expressions, std::uint8_t destination);
    void expression_list_fixed(const std::vector<ExprPtr>& expressions, std::uint8_t destination, std::uint16_t count);
    void arguments(const std::vector<ExprPtr>& arguments, std::uint8_t destination, std::uint8_t fixed_prefix, bool& open, std::uint16_t& count);
    LValue prepare_lvalue(const Expr& expr);
    void store_lvalue(const LValue& target, std::uint8_t value);
    std::uint32_t child_function(const FunctionBody& body, bool implicit_self, std::string name);

    void block(const Block& block, bool close_at_end = true);
    void statement(const Stmt& stmt);
    void while_statement(const Stmt& stmt, const WhileStmt& node);
    void repeat_statement(const Stmt& stmt, const RepeatStmt& node);
    void numeric_for(const Stmt& stmt, const NumericForStmt& node);
    void generic_for(const Stmt& stmt, const GenericForStmt& node);
    LoopEmit& loop(NodeId id);
    void finish_loop(LoopEmit& loop, std::size_t break_target, std::size_t continue_target);

    Compiler& owner_;
    const SemanticModel& semantic_;
    const FunctionInfo& info_;
    std::string name_;
    std::uint32_t next_child_index_ {0};
    std::vector<suru::ir::Word> code_;
    std::uint16_t temp_top_ {0};
    std::uint16_t high_water_ {0};
    std::vector<LoopEmit> loops_;
};

class Compiler {
public:
    Compiler(const SemanticModel& semantic, std::vector<Diagnostic>& diagnostics)
        : semantic(semantic), diagnostics(diagnostics) {}

    suru::ir::CodeUnit run(const Ast& ast) {
        unit.chunks.emplace_back();
        FunctionCompiler root(*this, ast.root.id, "<main>");
        unit.chunks[0] = root.compile(ast.root, 0, false);
        unit.entry_chunk = 0;
        return std::move(unit);
    }

    std::uint32_t constant(double value) {
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
        if (const auto it = numbers.find(bits); it != numbers.end()) return it->second;
        const auto id = static_cast<std::uint32_t>(unit.constants.size());
        if (id > suru::ir::bx_mask) {
            diagnostics.push_back({{}, "constant pool exceeds bytecode index range"});
            return 0;
        }
        unit.constants.push_back(suru::ir::NumberConstant {value});
        numbers.emplace(bits, id);
        return id;
    }

    std::uint32_t constant(std::string_view value) {
        if (const auto it = strings.find(std::string(value)); it != strings.end()) return it->second;
        const auto id = static_cast<std::uint32_t>(unit.constants.size());
        if (id > suru::ir::bx_mask) {
            diagnostics.push_back({{}, "constant pool exceeds bytecode index range"});
            return 0;
        }
        unit.constants.push_back(suru::ir::StringConstant {std::string(value)});
        strings.emplace(std::string(value), id);
        return id;
    }

    std::uint32_t compile_child(const FunctionBody& body, bool implicit_self, std::string name) {
        (void)implicit_self;
        const auto id = static_cast<std::uint32_t>(unit.chunks.size());
        if (id > suru::ir::bx_mask) {
            diagnostics.push_back({body.range.begin, "chunk count exceeds bytecode index range"});
            return 0;
        }
        unit.chunks.emplace_back();
        const auto found = semantic.functions.find(body.id);
        if (found == semantic.functions.end()) {
            diagnostics.push_back({body.range.begin, "missing function semantic information"});
            return id;
        }
        FunctionCompiler child(*this, body.id, std::move(name));
        unit.chunks[id] = child.compile(*body.block, found->second.arity, found->second.vararg);
        return id;
    }

    const SemanticModel& semantic;
    std::vector<Diagnostic>& diagnostics;
    suru::ir::CodeUnit unit;

private:
    std::unordered_map<std::uint64_t, std::uint32_t> numbers;
    std::unordered_map<std::string, std::uint32_t> strings;
};

FunctionCompiler::FunctionCompiler(Compiler& owner, NodeId body, std::string name)
    : owner_(owner), semantic_(owner.semantic), info_(semantic_.functions.at(body)), name_(std::move(name)),
      temp_top_(info_.local_slots), high_water_(info_.local_slots) {}

std::uint8_t FunctionCompiler::allocate(std::uint16_t count) {
    const std::uint16_t begin = temp_top_;
    ensure(temp_top_ + count);
    temp_top_ += count;
    high_water_ = std::max(high_water_, temp_top_);
    return static_cast<std::uint8_t>(std::min<std::uint16_t>(begin, 254));
}

void FunctionCompiler::ensure(std::uint16_t end) {
    if (end > 255) {
        owner_.diagnostics.push_back({{}, "function requires more than 255 registers during code generation"});
    }
    high_water_ = std::max(high_water_, std::min<std::uint16_t>(end, 255));
}

std::size_t FunctionCompiler::emit(suru::ir::Word word) {
    code_.push_back(word);
    return code_.size() - 1U;
}

std::size_t FunctionCompiler::jump() { return emit(suru::ir::encode_sax(suru::ir::Op::Jmp, 0)); }

void FunctionCompiler::patch(std::size_t at, std::size_t target) {
    const std::int64_t relative = static_cast<std::int64_t>(target) - static_cast<std::int64_t>(at) - 1;
    if (relative < -(1LL << 24) || relative > (1LL << 24) - 1) {
        owner_.diagnostics.push_back({{}, "jump range exceeds bytecode limit"});
        return;
    }
    code_[at] = suru::ir::encode_sax(suru::ir::Op::Jmp, static_cast<std::int32_t>(relative));
}

std::size_t FunctionCompiler::jump_if_falsy(std::uint8_t reg) {
    emit(suru::ir::encode_abx(suru::ir::Op::IfFalsy, reg, 0));
    return jump();
}

std::size_t FunctionCompiler::jump_if_truthy(std::uint8_t reg) {
    emit(suru::ir::encode_abx(suru::ir::Op::IfTruthy, reg, 0));
    return jump();
}

void FunctionCompiler::load_nil(std::uint8_t reg) { emit(suru::ir::encode_abx(suru::ir::Op::LoadNil, reg, 0)); }

void FunctionCompiler::load_number(std::uint8_t reg, double value) {
    if (value >= -65536.0 && value <= 65535.0 && value == static_cast<double>(static_cast<std::int64_t>(value))) {
        emit(suru::ir::encode_abx(suru::ir::Op::Load, reg,
            static_cast<std::uint32_t>(static_cast<std::int32_t>(value)) & suru::ir::bx_mask, true));
    } else emit(suru::ir::encode_abx(suru::ir::Op::LoadK, reg, owner_.constant(value)));
}

void FunctionCompiler::load_string(std::uint8_t reg, std::string_view value) {
    emit(suru::ir::encode_abx(suru::ir::Op::LoadK, reg, owner_.constant(value)));
}

const ResolvedBinding& FunctionCompiler::binding(NodeId id) const { return semantic_.bindings.at(id); }
const std::vector<std::uint8_t>& FunctionCompiler::declarations(NodeId id) const { return semantic_.declarations.at(id); }
std::uint8_t FunctionCompiler::block_base(NodeId id) const { return semantic_.block_bases.at(id); }

void FunctionCompiler::read_binding(const ResolvedBinding& value, std::uint8_t destination) {
    if (value.kind == BindingKind::Local) {
        if (value.index != destination) emit(suru::ir::encode_abx(suru::ir::Op::Load, destination, value.index));
    } else if (value.kind == BindingKind::Upvalue) {
        emit(suru::ir::encode_abx(suru::ir::Op::GetUpvalue, value.index, destination));
    } else {
        const std::uint32_t key = owner_.constant(value.global_name);
        if (key <= 255) emit(suru::ir::encode_abx(suru::ir::Op::GetGlobalK, key, destination));
        else {
            const std::uint16_t saved = temp_top_;
            const auto key_reg = allocate(); load_string(key_reg, value.global_name);
            emit(suru::ir::encode_abx(suru::ir::Op::GetGlobal, key_reg, destination)); reset(saved);
        }
    }
}

void FunctionCompiler::write_binding(const ResolvedBinding& value, std::uint8_t source) {
    if (value.kind == BindingKind::Local) {
        if (value.index != source) emit(suru::ir::encode_abx(suru::ir::Op::Load, value.index, source));
    } else if (value.kind == BindingKind::Upvalue) {
        emit(suru::ir::encode_abx(suru::ir::Op::SetUpvalue, value.index, source));
    } else {
        const std::uint32_t key = owner_.constant(value.global_name);
        if (key <= 255) emit(suru::ir::encode_abx(suru::ir::Op::SetGlobalK, key, source));
        else {
            const std::uint16_t saved = temp_top_;
            const auto key_reg = allocate(); load_string(key_reg, value.global_name);
            emit(suru::ir::encode_abx(suru::ir::Op::SetGlobal, key_reg, source)); reset(saved);
        }
    }
}

void FunctionCompiler::regular_tail(std::uint8_t destination, ResultMode mode) {
    if (mode.kind == ResultKind::Fixed) {
        ensure(static_cast<std::uint16_t>(destination) + mode.count);
        for (std::uint16_t i = 1; i < mode.count; ++i) load_nil(static_cast<std::uint8_t>(destination + i));
    }
}

void FunctionCompiler::expression(const Expr& expr, std::uint8_t destination, ResultMode mode) {
    if (mode.kind == ResultKind::Discard) mode = {ResultKind::Fixed, 0};
    const std::uint16_t reserved = static_cast<std::uint16_t>(destination)
        + (mode.kind == ResultKind::Fixed ? std::max<std::uint16_t>(mode.count, 1) : 1);
    if (temp_top_ < reserved) temp_top_ = reserved;
    ensure(reserved);
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, CallExpr>) call(expr, node, destination, mode);
        else if constexpr (std::is_same_v<T, MethodCallExpr>) method_call(expr, node, destination, mode);
        else if constexpr (std::is_same_v<T, VarargExpr>) {
            if (mode.kind == ResultKind::Open) emit(suru::ir::encode_abx(suru::ir::Op::Varg, destination, 0, true));
            else emit(suru::ir::encode_abx(suru::ir::Op::Varg, destination,
                mode.kind == ResultKind::Fixed ? mode.count : 1));
        } else if constexpr (std::is_same_v<T, NilExpr>) { load_nil(destination); regular_tail(destination, mode); }
        else if constexpr (std::is_same_v<T, BoolExpr>) {
            emit(suru::ir::encode_abx(node.value ? suru::ir::Op::LoadTrue : suru::ir::Op::LoadFalse, destination, 0));
            regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, NumberExpr>) {
            try { load_number(destination, std::stod(node.text)); }
            catch (...) { owner_.diagnostics.push_back({expr.range.begin, "invalid numeric literal"}); load_nil(destination); }
            regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, StringExpr>) { load_string(destination, node.value); regular_tail(destination, mode); }
        else if constexpr (std::is_same_v<T, NameExpr>) { read_binding(binding(expr.id), destination); regular_tail(destination, mode); }
        else if constexpr (std::is_same_v<T, GroupExpr>) {
            expression(*node.expression, destination, {ResultKind::One, 1}); regular_tail(destination, mode);
        }
        else if constexpr (std::is_same_v<T, UnaryExpr>) {
            const std::uint16_t saved = temp_top_;
            const auto operand = allocate(); expression(*node.operand, operand);
            if (node.op == "-") emit(suru::ir::encode_abx(suru::ir::Op::Neg, destination, operand));
            else if (node.op == "not") emit(suru::ir::encode_abx(suru::ir::Op::Not, destination, operand));
            else if (node.op == "#") emit(suru::ir::encode_abx(suru::ir::Op::Len, destination, operand));
            else if (node.op == "~") emit(suru::ir::encode_abc(suru::ir::Op::Bxor, destination, operand, suru::ir::c_mask, true));
            reset(saved); regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            const std::uint16_t saved = temp_top_;
            expression(*node.left, destination);
            if (node.op == "and" || node.op == "or") {
                const auto end = node.op == "and" ? jump_if_falsy(destination) : jump_if_truthy(destination);
                expression(*node.right, destination); patch(end, code_.size());
            } else {
                const auto right = allocate(); expression(*node.right, right);
                static const std::unordered_map<std::string, suru::ir::Op> ops {
                    {"+", suru::ir::Op::Add}, {"-", suru::ir::Op::Sub}, {"*", suru::ir::Op::Mul},
                    {"/", suru::ir::Op::Div}, {"//", suru::ir::Op::Idiv}, {"%", suru::ir::Op::Mod},
                    {"^^", suru::ir::Op::Pow}, {"..", suru::ir::Op::Concat}, {"==", suru::ir::Op::Eq},
                    {"!=", suru::ir::Op::Ne}, {"<", suru::ir::Op::Lt}, {"<=", suru::ir::Op::Le},
                    {">", suru::ir::Op::Gt}, {">=", suru::ir::Op::Ge}, {"&", suru::ir::Op::Band},
                    {"|", suru::ir::Op::Bor}, {"^", suru::ir::Op::Bxor}, {"<<", suru::ir::Op::Shl},
                    {">>", suru::ir::Op::Shr},
                };
                if (const auto it = ops.find(node.op); it != ops.end()) emit(suru::ir::encode_abc(it->second, destination, destination, right));
                else owner_.diagnostics.push_back({expr.range.begin, "unsupported binary operator: " + node.op});
            }
            reset(saved); regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, IndexExpr>) {
            const std::uint16_t saved = temp_top_; const auto object = allocate(); const auto key = allocate();
            expression(*node.object, object); expression(*node.key, key);
            emit(suru::ir::encode_abc(suru::ir::Op::GetIndex, destination, object, key)); reset(saved); regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, FieldExpr>) {
            const std::uint16_t saved = temp_top_; const auto object = allocate(); const auto key = allocate();
            expression(*node.object, object); load_string(key, node.name);
            emit(suru::ir::encode_abc(suru::ir::Op::GetIndex, destination, object, key)); reset(saved); regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, FunctionExpr>) {
            const auto child = child_function(*node.body, false, "lambda");
            emit(suru::ir::encode_abx(suru::ir::Op::Closure, destination, child)); regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, ArrayExpr>) {
            emit(suru::ir::encode_abx(suru::ir::Op::NewArray, destination, 0, true));
            if (!node.elements.empty()) {
                const std::uint16_t saved = temp_top_; const auto start = allocate(static_cast<std::uint16_t>(node.elements.size()));
                bool open = expression_list_open(node.elements, start);
                const std::uint16_t fixed = static_cast<std::uint16_t>(node.elements.size());
                emit(suru::ir::encode_abc(suru::ir::Op::PushArrayX, destination, start, open ? 0 : fixed, open)); reset(saved);
            }
            regular_tail(destination, mode);
        } else if constexpr (std::is_same_v<T, TableExpr>) {
            emit(suru::ir::encode_abx(suru::ir::Op::NewTable, destination, 0));
            for (const auto& field : node.fields) {
                const std::uint16_t saved = temp_top_; const auto key = allocate(); const auto value = allocate();
                expression(*field.key, key); expression(*field.value, value);
                emit(suru::ir::encode_abc(suru::ir::Op::SetIndex, destination, key, value)); reset(saved);
            }
            regular_tail(destination, mode);
        }
    }, expr.kind);
}

void FunctionCompiler::arguments(
    const std::vector<ExprPtr>& values, std::uint8_t destination, std::uint8_t fixed_prefix,
    bool& open, std::uint16_t& count
) {
    open = false;
    count = fixed_prefix;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto target = static_cast<std::uint8_t>(destination + fixed_prefix + i);
        ensure(static_cast<std::uint16_t>(target) + 1U);
        if (i + 1U == values.size() && multi_valued(*values[i])) {
            expression(*values[i], target, {ResultKind::Open, suru::ir::multret}); open = true;
        } else {
            expression(*values[i], target); ++count;
        }
    }
}

void FunctionCompiler::call(const Expr&, const CallExpr& node, std::uint8_t destination, ResultMode mode) {
    const std::uint16_t saved = temp_top_;
    ensure(destination + 1U); expression(*node.callee, destination);
    bool open = false; std::uint16_t argc = 0;
    arguments(node.arguments, static_cast<std::uint8_t>(destination + 1U), 0, open, argc);
    const std::uint16_t retc = mode.kind == ResultKind::Open ? suru::ir::multret
        : mode.kind == ResultKind::Fixed ? mode.count : mode.kind == ResultKind::Discard ? 0 : 1;
    emit(suru::ir::encode_abc(suru::ir::Op::Call, destination, open ? 0 : argc, retc, open));
    reset(saved);
}

void FunctionCompiler::method_call(const Expr&, const MethodCallExpr& node, std::uint8_t destination, ResultMode mode) {
    const std::uint16_t saved = temp_top_;
    ensure(destination + 3U);
    expression(*node.receiver, static_cast<std::uint8_t>(destination + 1U));
    load_string(static_cast<std::uint8_t>(destination + 2U), node.method);
    emit(suru::ir::encode_abc(suru::ir::Op::GetIndex, destination, destination + 1U, destination + 2U));
    bool open = false; std::uint16_t argc = 1;
    arguments(node.arguments, static_cast<std::uint8_t>(destination + 1U), 1, open, argc);
    const std::uint16_t retc = mode.kind == ResultKind::Open ? suru::ir::multret
        : mode.kind == ResultKind::Fixed ? mode.count : mode.kind == ResultKind::Discard ? 0 : 1;
    emit(suru::ir::encode_abc(suru::ir::Op::Call, destination, open ? 0 : argc, retc, open));
    reset(saved);
}

bool FunctionCompiler::expression_list_open(const std::vector<ExprPtr>& values, std::uint8_t destination) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto target = static_cast<std::uint8_t>(destination + i); ensure(target + 1U);
        if (i + 1U == values.size() && multi_valued(*values[i])) {
            expression(*values[i], target, {ResultKind::Open, suru::ir::multret}); return true;
        }
        expression(*values[i], target);
    }
    return false;
}

void FunctionCompiler::expression_list_fixed(
    const std::vector<ExprPtr>& values, std::uint8_t destination, std::uint16_t count
) {
    std::size_t index = 0;
    for (; index < values.size(); ++index) {
        const bool last = index + 1U == values.size();
        if (index < count) {
            const auto target = static_cast<std::uint8_t>(destination + index);
            const auto need = static_cast<std::uint16_t>(count - index);
            expression(*values[index], target, last ? ResultMode {ResultKind::Fixed, need} : ResultMode {});
        } else {
            const std::uint16_t saved = temp_top_; const auto scratch = allocate();
            expression(*values[index], scratch, {ResultKind::Discard, 0}); reset(saved);
        }
    }
    if (values.empty()) for (std::uint16_t i = 0; i < count; ++i) load_nil(static_cast<std::uint8_t>(destination + i));
    else if (values.size() < count && !multi_valued(*values.back())) {
        for (std::size_t i = values.size(); i < count; ++i) load_nil(static_cast<std::uint8_t>(destination + i));
    }
}

std::uint32_t FunctionCompiler::child_function(const FunctionBody& body, bool implicit_self, std::string name) {
    name += "@" + std::to_string(next_child_index_++);
    if (name_ != "<main>") name = name_ + "::" + name;
    return owner_.compile_child(body, implicit_self, std::move(name));
}

FunctionCompiler::LValue FunctionCompiler::prepare_lvalue(const Expr& expr) {
    if (std::holds_alternative<NameExpr>(expr.kind)) return {LValue::Kind::Binding, binding(expr.id), 0, 0};
    if (const auto* index = std::get_if<IndexExpr>(&expr.kind)) {
        const auto object = allocate(); const auto key = allocate();
        expression(*index->object, object); expression(*index->key, key);
        return {LValue::Kind::Index, {}, object, key};
    }
    const auto& field = std::get<FieldExpr>(expr.kind);
    const auto object = allocate(); const auto key = allocate();
    expression(*field.object, object); load_string(key, field.name);
    return {LValue::Kind::Index, {}, object, key};
}

void FunctionCompiler::store_lvalue(const LValue& target, std::uint8_t value) {
    if (target.kind == LValue::Kind::Binding) write_binding(target.binding, value);
    else emit(suru::ir::encode_abc(suru::ir::Op::SetIndex, target.object, target.key, value));
}

void FunctionCompiler::block(const Block& value, bool close_at_end) {
    for (const auto& stmt : value.statements) statement(*stmt);
    if (close_at_end) emit(suru::ir::encode_abx(suru::ir::Op::Close, block_base(value.id), 0));
}

FunctionCompiler::LoopEmit& FunctionCompiler::loop(NodeId id) {
    for (auto it = loops_.rbegin(); it != loops_.rend(); ++it) if (it->statement == id) return *it;
    throw std::logic_error("missing active loop");
}

void FunctionCompiler::finish_loop(LoopEmit& value, std::size_t break_target, std::size_t continue_target) {
    for (const auto at : value.breaks) patch(at, break_target);
    for (const auto at : value.continues) patch(at, continue_target);
}

void FunctionCompiler::while_statement(const Stmt& stmt, const WhileStmt& node) {
    const std::size_t head = code_.size(); const std::uint16_t saved = temp_top_; const auto condition = allocate();
    expression(*node.condition, condition); const auto exit = jump_if_falsy(condition); reset(saved);
    loops_.push_back({stmt.id, {}, {}}); block(*node.block); const auto& active = loops_.back();
    const std::size_t back = jump(); patch(back, head); const std::size_t end = code_.size(); patch(exit, end);
    LoopEmit done = active; loops_.pop_back(); finish_loop(done, end, head);
}

void FunctionCompiler::repeat_statement(const Stmt& stmt, const RepeatStmt& node) {
    const std::size_t start = code_.size(); loops_.push_back({stmt.id, {}, {}});
    block(*node.block, false); const std::size_t condition_target = code_.size();
    const std::uint16_t saved = temp_top_; const auto condition = allocate(); expression(*node.condition, condition);
    const auto again = jump_if_falsy(condition); reset(saved);
    emit(suru::ir::encode_abx(suru::ir::Op::Close, block_base(node.block->id), 0));
    const auto exit_jump = jump();
    const std::size_t repeat_close = code_.size();
    emit(suru::ir::encode_abx(suru::ir::Op::Close, block_base(node.block->id), 0));
    const auto back = jump(); patch(back, start); patch(again, repeat_close);
    const std::size_t end = code_.size(); patch(exit_jump, end);
    LoopEmit done = loops_.back(); loops_.pop_back(); finish_loop(done, end, condition_target);
}

void FunctionCompiler::numeric_for(const Stmt& stmt, const NumericForStmt& node) {
    const std::uint16_t saved = temp_top_; const auto limit = allocate(); const auto step = allocate(); const auto test = allocate();
    const auto var = declarations(stmt.id).at(0); expression(*node.initial, var); expression(*node.limit, limit);
    if (node.step) expression(*node.step, step); else load_number(step, 1);
    emit(suru::ir::encode_abc(suru::ir::Op::Eq, test, step, 0, true));
    const auto nonzero = jump_if_falsy(test);
    load_string(test, "numeric for step must not be zero"); emit(suru::ir::encode_abx(suru::ir::Op::Raise, test, 0));
    patch(nonzero, code_.size());
    const std::size_t head = code_.size();
    emit(suru::ir::encode_abc(suru::ir::Op::Gt, test, step, 0, true));
    const auto negative = jump_if_falsy(test);
    emit(suru::ir::encode_abc(suru::ir::Op::Lt, test, var, limit)); const auto positive_exit = jump_if_falsy(test);
    const auto body_jump = jump();
    const std::size_t negative_test = code_.size(); patch(negative, negative_test);
    emit(suru::ir::encode_abc(suru::ir::Op::Gt, test, var, limit)); const auto negative_exit = jump_if_falsy(test);
    const std::size_t body_target = code_.size(); patch(body_jump, body_target);
    loops_.push_back({stmt.id, {}, {}}); block(*node.block); const std::size_t increment = code_.size();
    emit(suru::ir::encode_abc(suru::ir::Op::Add, var, var, step)); const auto back = jump(); patch(back, head);
    const std::size_t end = code_.size(); patch(positive_exit, end); patch(negative_exit, end);
    LoopEmit done = loops_.back(); loops_.pop_back(); finish_loop(done, end, increment); reset(saved);
}

void FunctionCompiler::generic_for(const Stmt& stmt, const GenericForStmt& node) {
    const std::uint16_t saved = temp_top_; const auto triple = allocate(3); expression_list_fixed(node.expressions, triple, 3);
    const auto call_base = allocate(static_cast<std::uint16_t>(std::max<std::size_t>(3, node.names.size())));
    const auto nil_reg = allocate(); const auto test = allocate();
    const std::size_t head = code_.size();
    emit(suru::ir::encode_abx(suru::ir::Op::Load, call_base, triple));
    emit(suru::ir::encode_abx(suru::ir::Op::Load, call_base + 1U, triple + 1U));
    emit(suru::ir::encode_abx(suru::ir::Op::Load, call_base + 2U, triple + 2U));
    emit(suru::ir::encode_abc(suru::ir::Op::Call, call_base, 2, static_cast<std::uint16_t>(node.names.size())));
    load_nil(nil_reg); emit(suru::ir::encode_abc(suru::ir::Op::Eq, test, call_base, nil_reg));
    const auto end_jump = jump_if_truthy(test);
    emit(suru::ir::encode_abx(suru::ir::Op::Load, triple + 2U, call_base));
    const auto& slots = declarations(stmt.id);
    for (std::size_t i = 0; i < slots.size(); ++i) emit(suru::ir::encode_abx(suru::ir::Op::Load, slots[i], call_base + i));
    loops_.push_back({stmt.id, {}, {}}); block(*node.block); const std::size_t next = code_.size();
    const auto back = jump(); patch(back, head); const std::size_t end = code_.size(); patch(end_jump, end);
    LoopEmit done = loops_.back(); loops_.pop_back(); finish_loop(done, end, next); reset(saved);
}

void FunctionCompiler::statement(const Stmt& stmt) {
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, BreakStmt> || std::is_same_v<T, ContinueStmt>) {
            const auto target = semantic_.loop_targets.at(stmt.id);
            if (target.close_base) emit(suru::ir::encode_abx(suru::ir::Op::Close, *target.close_base, 0));
            auto& active = loop(target.loop_statement);
            (std::is_same_v<T, BreakStmt> ? active.breaks : active.continues).push_back(jump());
        } else if constexpr (std::is_same_v<T, DoStmt>) block(*node.block);
        else if constexpr (std::is_same_v<T, WhileStmt>) while_statement(stmt, node);
        else if constexpr (std::is_same_v<T, RepeatStmt>) repeat_statement(stmt, node);
        else if constexpr (std::is_same_v<T, IfStmt>) {
            std::vector<std::size_t> ends;
            for (const auto& branch : node.branches) {
                const std::uint16_t saved = temp_top_; const auto cond = allocate(); expression(*branch.condition, cond);
                const auto next = jump_if_falsy(cond); reset(saved); block(*branch.block); ends.push_back(jump()); patch(next, code_.size());
            }
            if (node.else_block) block(*node.else_block);
            for (const auto at : ends) patch(at, code_.size());
        } else if constexpr (std::is_same_v<T, NumericForStmt>) numeric_for(stmt, node);
        else if constexpr (std::is_same_v<T, GenericForStmt>) generic_for(stmt, node);
        else if constexpr (std::is_same_v<T, FunctionStmt>) {
            const std::uint16_t saved = temp_top_; const auto closure = allocate();
            const auto child = child_function(*node.body, node.name.method.has_value(), node.name.method ? *node.name.method : node.name.path.back());
            emit(suru::ir::encode_abx(suru::ir::Op::Closure, closure, child));
            if (node.name.path.size() == 1 && !node.name.method) {
                write_binding(semantic_.function_roots.at(stmt.id), closure);
            } else {
                const auto object = allocate(); read_binding(semantic_.function_roots.at(stmt.id), object);
                const std::size_t stop = node.name.method ? node.name.path.size() : node.name.path.size() - 1U;
                for (std::size_t i = 1; i < stop; ++i) {
                    const auto key = allocate(); load_string(key, node.name.path[i]);
                    emit(suru::ir::encode_abc(suru::ir::Op::GetIndex, object, object, key));
                }
                const auto key = allocate(); load_string(key, node.name.method ? *node.name.method : node.name.path.back());
                emit(suru::ir::encode_abc(suru::ir::Op::SetIndex, object, key, closure));
            }
            reset(saved);
        } else if constexpr (std::is_same_v<T, LocalFunctionStmt>) {
            const auto slot = declarations(stmt.id).at(0); const auto child = child_function(*node.body, false, node.name);
            emit(suru::ir::encode_abx(suru::ir::Op::Closure, slot, child));
        } else if constexpr (std::is_same_v<T, LocalDeclStmt>) {
            const auto& slots = declarations(stmt.id); const std::uint16_t saved = temp_top_;
            const auto values = allocate(static_cast<std::uint16_t>(slots.size()));
            expression_list_fixed(node.expressions, values, static_cast<std::uint16_t>(slots.size()));
            for (std::size_t i = 0; i < slots.size(); ++i) emit(suru::ir::encode_abx(suru::ir::Op::Load, slots[i], values + i));
            reset(saved);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            if (node.expressions.empty()) {
                emit(suru::ir::encode_abx(suru::ir::Op::Close, 0, 0));
                emit(suru::ir::encode_abx(suru::ir::Op::Return, 0, 0));
            }
            else {
                const std::uint16_t saved = temp_top_; const auto start = allocate(static_cast<std::uint16_t>(node.expressions.size()));
                const bool open = expression_list_open(node.expressions, start);
                emit(suru::ir::encode_abx(suru::ir::Op::Close, 0, 0));
                emit(suru::ir::encode_abx(suru::ir::Op::Return, start,
                    open ? 0 : static_cast<std::uint16_t>(node.expressions.size()), open)); reset(saved);
            }
        } else if constexpr (std::is_same_v<T, AssignmentStmt>) {
            const std::uint16_t saved = temp_top_; const auto values = allocate(static_cast<std::uint16_t>(node.variables.size()));
            expression_list_fixed(node.expressions, values, static_cast<std::uint16_t>(node.variables.size()));
            std::vector<LValue> targets; targets.reserve(node.variables.size());
            for (const auto& variable : node.variables) targets.push_back(prepare_lvalue(*variable));
            for (std::size_t i = 0; i < targets.size(); ++i) {
                store_lvalue(targets[i], static_cast<std::uint8_t>(values + i));
            }
            reset(saved);
        } else if constexpr (std::is_same_v<T, CallStmt>) {
            const std::uint16_t saved = temp_top_; const auto scratch = allocate();
            expression(*node.call, scratch, {ResultKind::Discard, 0}); reset(saved);
        }
    }, stmt.kind);
}

suru::ir::Chunk FunctionCompiler::compile(const Block& root, std::uint8_t arity, bool vararg) {
    if (vararg) emit(suru::ir::encode_abx(suru::ir::Op::VargPrep, arity, 0));
    block(root);
    emit(suru::ir::encode_abx(suru::ir::Op::Return, 0, 0));
    return suru::ir::Chunk {
        std::move(name_), vararg ? std::uint8_t {255} : arity,
        static_cast<std::uint8_t>(std::max<std::uint16_t>(high_water_, 1)),
        info_.upvalues, std::move(code_)
    };
}

} // namespace

CompileResult compile(const Ast& ast, const SemanticModel& semantics) {
    CompileResult result;
    result.semantics = semantics;
    Compiler compiler(result.semantics, result.diagnostics);
    result.code = compiler.run(ast);
    return result;
}

CompileResult compile(const Ast& ast) {
    ResolveResult resolved = resolve(ast);
    CompileResult result;
    result.semantics = std::move(resolved.model);
    result.diagnostics = std::move(resolved.diagnostics);
    if (!result.diagnostics.empty()) return result;
    Compiler compiler(result.semantics, result.diagnostics);
    result.code = compiler.run(ast);
    return result;
}

} // namespace suru::front

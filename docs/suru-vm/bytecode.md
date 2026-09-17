# Suru VM Bytecode

## Image and runtime forms

`suru::ir::Chunk` owns its `vector<Word> code`. `suru::ir::CodeUnit` owns number/string constants, chunks, and an entry chunk index. It is independent of any VM.

`VM::load_code_unit()` interns strings, converts constants to `Value`, flattens chunk code, builds runtime code ranges, and returns the entry closure.

## Word formats

Every instruction is one 32-bit word.

```text
ABC: op(6) | i(1) | A(8) | B(8) | C(9)
ABx: op(6) | i(1) | A(8) | Bx(17)
sAx: op(6) | signed offset(25)
```

Immediate values are signed: C is 9-bit, B is 8-bit, and Bx is 17-bit. Assembly prefixes an immediate with `#`.

## Calls, returns, and varargs

```text
CALL F argc retc
CALL.v F retc
RETURN A count
RETURN.v A
VARGPREP fixed
VARG A count
VARG.v A
```

`CALL.v` uses `R[F+1..top)`. `RETURN.v` and `VARG.v` also use dynamic `top` ranges.

Assembler return count is `0..510` or `@vret`. `@vret` encodes the internal C field sentinel `0x1ff`; numeric `511` and `0x1ff` source spellings are rejected.

Chunk arity is `0..254` or `@va` (`255`). An `@va` call initially preserves all arguments. `VARGPREP` may appear anywhere and execute any number of times; each execution interprets the current `[base+1, top)` as its input and creates a new register bank. The compiler normally emits it once as a prologue.

## Index operations

Table and array access share two opcodes.

```text
GETINDEX dst object key
SETINDEX object key value
```

`GETINDEX` accepts a signed C immediate key. `SETINDEX` accepts a signed B immediate key; its value is always a register. Runtime object kind chooses table or array behavior.

Table rejects nil and NaN keys. Missing valid keys read as nil. Array keys must be in-range integers; negative indices count from the end.

## Arrays

```text
NEWARRAY dst length
PUSHARRAYX array first count
PUSHARRAYX.v array first
```

`PUSHARRAYX` appends a fixed register range. Its `.v` form appends through dynamic top. Both keep top unchanged.

## Closures and errors

`CLOSURE A chunk` captures the target chunk's ordered `UpvalueInfo` list. A source is either a local register or the current closure's upvalue.

`CLOSE A` closes open upvalues in current register range `[R[A], R[slots])`; `A == slots` is valid and empty.

`RAISE A` throws `RaisedError` with `R[A]` preserved as its payload. C functions use `VM::raise(Value)` for the same behavior.

## Opcode summary

| Group | Opcodes |
| --- | --- |
| Literals/move | `LOAD`, `LOADNIL`, `LOADTRUE`, `LOADFALSE`, `LOADK` |
| Globals | `GETGLOBALK`, `SETGLOBALK`, `GETGLOBAL`, `SETGLOBAL` |
| Arithmetic | `ADD`, `SUB`, `MUL`, `DIV`, `IDIV`, `MOD`, `POW`, `NEG` |
| Other values | `CONCAT`, `NOT`, `LEN`, `AND`, `OR` |
| Comparison | `EQ`, `NE`, `LT`, `LE`, `GT`, `GE` |
| Bit operations | `BAND`, `BOR`, `BXOR`, `SHL`, `SHR` |
| Aggregate | `NEWTABLE`, `NEWARRAY`, `GETINDEX`, `SETINDEX`, `PUSHARRAYX` |
| Control | `JMP`, `IFFALSY`, `IFTRUTHY`, `IFEQ`, `IFNE`, `IFLT`, `IFLE`, `IFGT`, `IFGE` |
| Calls | `CALL`, `RETURN`, `VARGPREP`, `VARG` |
| Closures | `CLOSURE`, `GETUPVAL`, `SETUPVAL`, `CLOSE` |
| Errors | `RAISE` |

## Assembly directives

```sura
.const
k0 = number 1
k1 = string "name"
.entry main

.chunk main 0 2
    RETURN 0 0

.chunk child @va 4
.upvalue local 0
    VARGPREP 1
    RETURN.v 0
```

Whole-unit disassembly emits `.const`, `.entry`, `.chunk`, `.upvalue`, generated jump labels, `.v`, `@va`, and `@vret`. Its output can be assembled again.

Assembly comments start with `--` or `;` outside quoted strings.

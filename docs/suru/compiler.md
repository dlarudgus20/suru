# Suru Frontend and Compiler

## Library boundary

`libsuru` owns lexer, typed AST, parser, YAML dump, resolver, semantic side tables, and direct codegen. It depends on `libsuru-ir`, but not on the VM.

`libsuru-ir` owns `suru::ir::*`: constants, chunks, instruction codec, builder, assembler/disassembler, and binary I/O.

`libsuru-vm` materializes an `ir::CodeUnit` into runtime values and flattened runtime code through `VM::load_code_unit()`.

## Typed AST

Expressions and statements are `std::variant` nodes with stable `NodeId` and source range. The root is always a `Block`. YAML dumping visits these typed nodes; code generation does not parse dump strings.

## Semantic model

Resolver output is stored separately from syntax.

- `NodeId -> ResolvedBinding` for local, upvalue, and global names
- `FunctionBody NodeId -> FunctionInfo` for arity, vararg, local slots, and upvalue descriptors
- declaration slots and block close boundaries
- `break`/`continue` target loop and close boundary

Normal local initializers resolve before the new names are published. Local function bindings are published before their bodies resolve.

## Direct code generation

The compiler emits `ir::CodeUnit` directly from AST plus semantic tables. There is no intermediate frontend IR.

Expression results use four contexts.

| Mode | Meaning |
| --- | --- |
| One | One adjusted value |
| Fixed(N) | Exactly N values, padded or truncated |
| Open | Preserve the dynamic tail |
| Discard | Evaluate effects and request no call results |

Local registers are stable for a function. Temporary registers start after all resolved local slots, and the high-water mark becomes `Chunk::slots`.

Control-flow jumps are patched after block emission. Lexical exits emit `CLOSE` at the resolver-provided boundary. Vararg functions emit `VARGPREP` in the compiler prologue even though the bytecode instruction itself has no placement restriction.

# Suru Frontend and Compiler

## Library boundary

`libsuru` owns lexer, typed AST, parser, YAML dump, resolver, semantic side tables, and direct codegen. It depends on `libsuru-ir`, but not on the VM.

`libsuru-ir` owns `suru::ir::*`: constants, chunks, instruction codec, builder, assembler/disassembler, and binary I/O.

`libsuru-vm` materializes an `ir::CodeUnit` into runtime values and flattened runtime code through `VM::load_code_unit()`.

## Typed AST

Expressions and statements are `std::variant` nodes with stable `NodeId` and source range. The root is always a `Block`. YAML dumping visits these typed nodes; code generation does not parse dump strings.

`SourceRange` is a half-open interval `[begin, end)`. Locations use 1-based lines and byte columns. Tokens record their actual consumed end, including multiline strings. Composite nodes include their full syntax and delimiters. Blocks span their first through last statement; an empty block is zero-width at its terminator or EOF. Function bodies span `(` through the closing `end`. YAML exposes both `range.begin` and `range.end`.

## Semantic model

Resolver output is stored separately from syntax.

- `NodeId -> ResolvedBinding` for local, upvalue, and global names
- `FunctionBody NodeId -> FunctionInfo` for arity, vararg, local slots, and upvalue descriptors
- declaration slots and block close boundaries
- `break`/`continue` target loop and close boundary

Normal local initializers resolve before the new names are published. Local function bindings are published before their bodies resolve.

Loop and block-close state belongs to each function. Nested functions retain lexical capture access, but cannot target an enclosing function's loop or label. Each function can capture at most 255 upvalues; the resolver rejects overflow before narrowing an index.

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

## Chunk names

The root chunk is `<main>`. Direct children are named `<source-name>@<child-index>`, and nested children append `::<source-name>@<child-index>` to the parent name, for example `outer@0::inner@2`. Anonymous functions use `lambda`. Each index is the 0-based direct-child prototype ordinal within its parent, independent of AST NodeId and the flat `CodeUnit::chunks` index. `<main>` is omitted from child prefixes. Method chunks use the method name.

Chunk names are assembly/debug symbols. Names must be nonempty and unique within an image; hand-written assembly need not use compiler naming conventions. Execution and `CLOSURE` references continue to use numeric flat chunk indices.

An unclosed quoted string is tokenized as `UnterminatedString` rather than `Unknown`. In valid expression, call-argument, or literal-key positions, the parser reports `ParseStatus::Incomplete` and the opening quote location. Incremental parsing preserves the source buffer until the closing quote arrives; genuinely invalid syntax remains an error. Batch input reports the incomplete string and fails instead of waiting for more input.

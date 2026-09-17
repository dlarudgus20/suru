# Suru CLI and REPL

## Input selection

`--in=src|ir|sbc` explicitly selects source, assembly text, or binary SBC. An explicit format overrides the filename. Without it, case-insensitive `.sura` selects IR, `.sbc` selects SBC, and every other extension (including none) selects source. Stdin `-` and REPL default to source. Contents and magic bytes never select or override the input format; SBC magic is validated only after SBC has been selected.

## File and batch modes

| Output mode | Allowed input | Behavior |
| --- | --- | --- |
| None | src, ir, sbc | Compile, assemble, or load, then execute |
| `--ast` | src | Write typed AST as YAML |
| `--ir` | src, ir | Compile or assemble, then write `.sura` text |
| `--sbc` | src, ir | Compile or assemble, then save binary SBC; `-o` required |
| `--disas` | sbc | Load SBC, then write `.sura` text |

Output modes are mutually exclusive and cannot be repeated. `--in` cannot be repeated. Text output goes to stdout unless `-o` selects a file. Execute mode rejects `-o`. Binary output always requires a file opened in binary mode; binary stdout is unsupported on every platform.

```sh
suru script.suru
suru program.sura
suru program.sbc
suru --in=ir program.txt
suru --ast script.suru
suru --ir script.suru -o program.sura
suru --sbc script.suru -o program.sbc
suru --in=ir --sbc program.txt -o program.sbc
suru --disas program.sbc
suru --in=sbc --disas program.bin -o program.sura
```

Batch stdin is explicit and accepts source or IR text. Binary SBC stdin is rejected before reading input.

```sh
suru -
suru --ast -
suru --ir -
suru --sbc - -o program.sbc
suru --in=ir -
suru --in=ir --sbc - -o program.sbc
```

Option and combination errors exit with code 2. Parsing, assembly, image, and runtime errors exit with code 1. Assembly diagnostics include the input line number. Prompts and diagnostics go to stderr.

## REPL modes

Without an input path, only source execute, `--ast`, and `--ir` start a REPL. `--sbc`, `--disas`, IR/SBC input, and `-o` are rejected before reading input.

| Command | REPL output |
| --- | --- |
| `suru` | Compile and execute each complete input |
| `suru --ast` | YAML AST |
| `suru --ir` | Reassemblable `.sura` text |

`> ` is the normal prompt and `>> ` indicates incomplete input. `.exit` and `.quit` exit when entered as a fresh input. Execute REPL keeps one VM, so globals survive between entries; source-local bindings do not.

Assembly, binary serialization, execution, and disassembly are all provided by `suru`; no separate bytecode utility is built.

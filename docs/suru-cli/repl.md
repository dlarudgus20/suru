# Suru CLI and REPL

## File and batch modes

| Command | Behavior |
| --- | --- |
| `suru file.suru` | Compile and execute source |
| `suru file.sbc` | Load binary IR and execute |
| `suru --ast file.suru` | Write typed AST as YAML |
| `suru --ast file.suru -o out.yaml` | Save YAML AST |
| `suru --ir file.suru` | Write binary IR to stdout |
| `suru --ir file.suru -o out.sbc` | Save binary IR |

`--ast` and `--ir` are mutually exclusive. Execute mode rejects `-o`.

Batch stdin is explicit.

```sh
suru -
suru --ast -
suru --ir - -o out.sbc
```

## REPL modes

If no input path is present, the selected mode starts a REPL.

| Command | REPL output |
| --- | --- |
| `suru` | Compile and execute each complete input |
| `suru --ast` | YAML AST |
| `suru --ir` | Reassemblable `.sura` disassembly |

Prompts go to stderr, so stdout can be redirected cleanly. `> ` is the normal prompt and `>> ` indicates incomplete input. `.exit` and `.quit` exit when entered as a fresh input.

`-o` is not accepted in REPL mode. Execute REPL keeps one VM, so globals survive between entries; source-local bindings do not.

## Bytecode utility

`suru-bc` is the thin assembly/image utility.

```sh
suru-bc input.sura
suru-bc input.sura -o output.sbc
suru-bc input.sbc
suru-bc --disassemble input.sbc -o output.sura
```

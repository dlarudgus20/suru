# Suru Diagnostics

Parser and compiler diagnostics use this form.

```text
<path>:<line>:<column>: error: <message>
```

REPL input uses `<repl>` and batch stdin uses `<stdin>`.

`ParseStatus::Incomplete` means a closing token or block terminator may still arrive. The REPL requests another line; complete file input reports the parser diagnostic.

Resolver/compiler diagnostics include invalid vararg use, unknown loop labels, register/upvalue limits, and a constant numeric-for step of zero.

VM failures are C++ `RuntimeError` subclasses. The CLIs currently print:

```text
runtime error: <message>
```

Assembly syntax failures include their `.sura` line number. Binary image failures are reported as IR/image errors.

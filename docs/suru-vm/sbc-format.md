# Suru SBC Binary Format

`.sbc` stores a VM-independent `suru::ir::CodeUnit`. All integers and IEEE-754 f64 values are little-endian.

There is deliberately no version field and no compatibility reader. This project is still defining the format; after a layout change, regenerate images with the current tools.

## Layout

```text
magic               u32 = bytes 00 53 42 43
constant_count      u32
chunk_count         u32
entry_chunk         u32

constants[constant_count]
chunks[chunk_count]
```

Each constant starts with a tag.

| Tag | Payload |
| --- | --- |
| 1 | `f64` number |
| 2 | `u32 byte_length`, UTF-8 bytes |

Each chunk is stored as follows.

```text
name                string
arity               u8; 255 means @va
slots               u8
upvalue_count       u8
upvalues            repeated source:u8, index:u8
code_word_count     u32
code_words          repeated u32
```

Each chunk owns its code words in the image; `code_begin`, `code_end`, and a global code array are runtime materialization details and do not appear in `.sbc`.

## Validation

The reader rejects invalid magic, unsupported constant/upvalue tags, truncated or trailing bytes, missing/out-of-range entry chunks, fixed arity greater than slots, invalid constant/chunk/register references, invalid capture references at each `CLOSURE`, and unknown opcodes.

Chunk names must be nonempty and unique within a `CodeUnit`; they are assembly/debug symbols, while VM execution uses numeric chunk indices. Each chunk has at most 255 upvalues. `ir::UpvalueCount` is `std::uint8_t`; vector sizes are checked before narrowing. The runtime closure count field remains `size_t`.

IR image errors become `InvalidImageError` when materialized by the VM.

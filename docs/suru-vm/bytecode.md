# Suru VM Bytecode

## 목적과 범위
이 문서는 `libsuru-vm`의 현재 register-based 바이트코드 형식을 정의한다.
기준 구현:
- `libsuru-vm/include/suru/vm/opcode.hpp`
- `libsuru-vm/src/vm_exec.cpp`
- `suru-bc/src/main.cpp`

## 인코딩 규칙
- Opcode: 1바이트 (`u8`)
- 정수 인자: 가변 길이 부호 없는 정수 인코딩
- 점프 오프셋: `i16` (2바이트)
- 멀티바이트 정수 바이트 순서는 리틀엔디안으로 통일한다.

## CodeUnit / Chunk
`.chunk <name> <arity> <slots> <upvalues>`
- `arity` (`u8`): 파라미터 개수
- `slots` (`u8`): 프레임 레지스터 개수
- `upvalues` (`u8`): 클로저 캡처 개수

제약:
- `arity <= slots`
- `code_begin`, `code_end`, chunk 인덱스, 상수 인덱스는 `u32` 범위를 사용한다.

## 레지스터/호출 규약
- 현재 프레임의 `R[x]`는 `v_stack_[base + x]`에 대응한다.
- `CALL F argc retc`
  - callee: `R[F]`
  - args: `R[F+1] .. R[F+argc]`
  - result write-back: `R[F] .. R[F+retc-1]`
- 바이트코드 함수 인자 개수 보정
  - 부족한 인자는 `nil`로 채움
  - 초과 인자는 무시

## Opcode 목록
| Opcode | Byte | Arguments | Argument Type | 동작 |
| --- | --- | --- | --- | --- |
| MOVE | `0x01` | `A B` | `u8, u8` | `R[A] = R[B]` |
| LOADNIL | `0x02` | `A` | `u8` | `R[A] = nil` |
| LOADTRUE | `0x03` | `A` | `u8` | `R[A] = true` |
| LOADFALSE | `0x04` | `A` | `u8` | `R[A] = false` |
| LOADK | `0x05` | `A K` | `u8, u32` | `R[A] = constants[K]` |
| GETGLOBAL | `0x08` | `A K` | `u8, u32` | `R[A] = globals[constants[K]]` |
| SETGLOBAL | `0x09` | `K A` | `u32, u8` | `globals[constants[K]] = R[A]` |
| ADD/SUB/MUL/DIV/IDIV/MOD/POW | `0x10..0x16` | `A B C` | `u8, u8, u8` | 산술 연산 |
| NEG/NOT | `0x17..0x18` | `A B` | `u8, u8` | 단항 연산 |
| AND/OR | `0x19..0x1A` | `A B C` | `u8, u8, u8` | bool 논리 연산 |
| EQ/NE/LT/LE/GT/GE | `0x20..0x25` | `A B C` | `u8, u8, u8` | 비교 결과(bool) |
| BAND/BOR/BXOR/SHL/SHR | `0x30..0x34` | `A B C` | `u8, u8, u8` | 비트 연산 |
| NEWTABLE | `0x40` | `A` | `u8` | `R[A] = {}` |
| GETTABLE | `0x41` | `A B C` | `u8, u8, u8` | `R[A] = R[B][R[C]]` |
| SETTABLE | `0x42` | `A B C` | `u8, u8, u8` | `R[A][R[B]] = R[C]` |
| JMP | `0x50` | `rel` | `i16` | 상대 점프 |
| JMPIF | `0x51` | `A rel` | `u8, i16` | `R[A]`가 falsey면 점프 |
| CALL | `0x60` | `F argc retc` | `u8, u8, u8` | 함수 호출 |
| RETURN | `0x61` | `A retc` | `u8, u8` | `R[A..]` 반환 |
| CLOSURE | `0x62` | `A chunk` | `u8, u32` | `R[A]`에 closure 생성 |
| GETUPVAL | `0x63` | `A U` | `u8, u8` | `R[A] = upvalue[U]` |
| SETUPVAL | `0x64` | `U A` | `u8, u8` | `upvalue[U] = R[A]` |

## CLOSURE 캡처 규약
`CLOSURE A chunk`에서 `chunk.upvalues = n`이면:
- `upvalue[0] = R[A+1]`
- ...
- `upvalue[n-1] = R[A+n]`

## 어셈블리 (`.sura`) 규칙
- 섹션:
  - `.const`
  - `.chunk <name> <arity> <slots> <upvalues>`
- 라벨: `name:`
- 주석: `;` 이후 텍스트
- 엔트리: `main` chunk
- 각 chunk는 명시적 `RETURN`으로 끝나야 한다. 끝까지 도달하면 `unexpected end of chunk` 오류가 발생한다.

## 주요 런타임 오류
- `register index out of bounds`
- `constant index out of bounds`
- `global key must be string`
- `call register index out of bounds`
- `call argument range out of bounds`
- `call return range out of bounds`
- `upvalue index out of bounds`
- `jump target out of bounds`
- `unknown opcode`

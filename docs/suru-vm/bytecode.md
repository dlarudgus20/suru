# Suru VM Bytecode

## 목적과 범위
이 문서는 현재 `libsuru-vm`의 **register-based ISA**를 정의한다.
기준 구현:
- `libsuru-vm/include/suru/vm/opcode.hpp`
- `libsuru-vm/src/vm_exec.cpp`
- `suru-bc/src/main.cpp`

## 인코딩
- Opcode: 1바이트
- 정수 인자: ULEB128
- 점프 오프셋: SLEB (현재 `suru-bc`는 고정 5바이트 SLEB32 인코딩)

## CodeUnit / Chunk
`Chunk`는 `.chunk <name> <arity> <slots> <upvalues>`에 대응한다.
- `arity`: 파라미터 수
- `slots`: 프레임 레지스터 개수
- `upvalues`: 클로저 캡처 값 수

제약: `arity <= slots`

## 프레임 / 레지스터 규약
- 현재 함수 프레임의 레지스터 `R[x]`는 내부적으로 `v_stack_[base + x]`에 매핑된다.
- `CALL F argc retc`:
  - callee: `R[F]`
  - args: `R[F+1] .. R[F+argc]`
  - 결과: `R[F] .. R[F+retc-1]`
- 바이트코드 함수 인자 전달:
  - 부족 인자: `nil` 패딩
  - 초과 인자: 버림

## Opcode 포맷
| Opcode | Byte | Arguments | Argument Size | 동작 |
| --- | --- | --- | --- | --- |
| MOVE | `0x01` | `A B` | ULEB, ULEB | `R[A] = R[B]` |
| LOADNIL | `0x02` | `A` | ULEB | `R[A] = nil` |
| LOADTRUE | `0x03` | `A` | ULEB | `R[A] = true` |
| LOADFALSE | `0x04` | `A` | ULEB | `R[A] = false` |
| LOADK | `0x05` | `A K` | ULEB, ULEB | `R[A] = constants[K]` |
| GETGLOBAL | `0x08` | `A K` | ULEB, ULEB | `R[A] = globals[constants[K]]` |
| SETGLOBAL | `0x09` | `K A` | ULEB, ULEB | `globals[constants[K]] = R[A]` |
| ADD/SUB/MUL/DIV/IDIV/MOD/POW | `0x10..0x16` | `A B C` | ULEB x3 | `R[A] = R[B] (op) R[C]` |
| NEG/NOT | `0x17..0x18` | `A B` | ULEB x2 | `R[A] = op(R[B])` |
| AND/OR | `0x19..0x1A` | `A B C` | ULEB x3 | bool 논리연산 |
| EQ/NE/LT/LE/GT/GE | `0x20..0x25` | `A B C` | ULEB x3 | 비교 결과(bool) 저장 |
| BAND/BOR/BXOR/SHL/SHR | `0x30..0x34` | `A B C` | ULEB x3 | 비트 연산 |
| NEWTABLE | `0x40` | `A` | ULEB | `R[A] = {}` |
| GETTABLE | `0x41` | `A B C` | ULEB x3 | `R[A] = R[B][R[C]]` |
| SETTABLE | `0x42` | `A B C` | ULEB x3 | `R[A][R[B]] = R[C]` |
| JMP | `0x50` | `rel` | SLEB | 상대 점프 |
| JMPIF | `0x51` | `A rel` | ULEB, SLEB | `R[A]`가 falsey면 점프 |
| CALL | `0x60` | `F argc retc` | ULEB x3 | 함수 호출 |
| RETURN | `0x61` | `A retc` | ULEB x2 | `R[A..]` 반환 |
| CLOSURE | `0x62` | `A chunk` | ULEB x2 | `R[A]`에 closure 생성 |
| GETUPVAL | `0x63` | `A U` | ULEB x2 | `R[A] = upvalue[U]` |
| SETUPVAL | `0x64` | `U A` | ULEB x2 | `upvalue[U] = R[A]` |

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
- 엔트리 포인트: `main` chunk
- 각 chunk는 명시적 `RETURN`으로 종료해야 한다. chunk 끝까지 도달하면 `unexpected end of chunk` 런타임 오류가 발생한다.

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

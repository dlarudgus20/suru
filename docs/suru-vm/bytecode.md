# Suru VM Bytecode

## 목적과 범위
이 문서는 현재 `libsuru-vm` 구현이 실제로 실행하는 바이트코드 형식을 정의한다.
기준 구현:
- `libsuru-vm/include/suru/vm/opcode.hpp`
- `libsuru-vm/src/vm_exec.cpp`
- `suru-bc/src/main.cpp`

## 공통 인코딩
- Opcode: 1바이트 (`Op` enum 값)
- ULEB128: 부호 없는 가변 길이 정수
- SLEB: 부호 있는 가변 길이 정수
- 점프 인자 주의:
  - VM(`read_sleb`)은 가변 길이 SLEB를 디코드한다.
  - 현재 `suru-bc`는 점프 인자를 고정 5바이트 SLEB32로 인코드한다.

## CodeUnit / Chunk
`CodeUnit` 필드:
- `constants_`: 상수 풀 (`Value`)
- `opcodes_`: 바이트코드 스트림
- `chunks_`: 함수 단위 메타데이터

`Chunk` 필드:
- `name`
- `code_begin`, `code_end` (`[begin, end)`)
- `arity`
- `slots`
- `upvalues`

제약:
- `arity <= slots`

## 스택/프레임
- 값 스택: `v_stack_`
- 프레임 스택: `i_stack_`
- 로컬 접근: `slot = base + idx`
- 호출 전 스택 형태: `[..., callee, arg0, arg1, ...]`

## Opcode 요약 표
| Opcode | Byte | Arguments | Arg Size | Stack Effect | 동작 |
| --- | --- | --- | --- | --- | --- |
| POP | `0x01` | - | 0 | pop 1 | top 제거 |
| NIL | `0x02` | - | 0 | push 1 | `nil` push |
| TRUE | `0x03` | - | 0 | push 1 | `true` push |
| FALSE | `0x04` | - | 0 | push 1 | `false` push |
| CONST | `0x05` | `idx` | ULEB128 | push 1 | `constants_[idx]` push |
| GET_LOCAL | `0x06` | `idx` | ULEB128 | push 1 | `slot(base+idx)` 조회 |
| SET_LOCAL | `0x07` | `idx` | ULEB128 | pop 1 | `slot(base+idx)` 저장 |
| GET_GLOBAL | `0x08` | `idx` | ULEB128 | push 1 | `globals[key]` 조회 |
| SET_GLOBAL | `0x09` | `idx` | ULEB128 | pop 1 | `globals[key]` 저장 |
| ADD | `0x10` | - | 0 | pop 2, push 1 | 숫자 덧셈 |
| SUB | `0x11` | - | 0 | pop 2, push 1 | 숫자 뺄셈 |
| MUL | `0x12` | - | 0 | pop 2, push 1 | 숫자 곱셈 |
| DIV | `0x13` | - | 0 | pop 2, push 1 | 숫자 나눗셈 |
| IDIV | `0x14` | - | 0 | pop 2, push 1 | `floor(lhs / rhs)` |
| MOD | `0x15` | - | 0 | pop 2, push 1 | `fmod(lhs, rhs)` |
| POW | `0x16` | - | 0 | pop 2, push 1 | `pow(lhs, rhs)` |
| NEG | `0x17` | - | 0 | pop 1, push 1 | 단항 음수 |
| NOT | `0x18` | - | 0 | pop 1, push 1 | falsey 부정 |
| EQ | `0x20` | - | 0 | pop 2, push 1 | 동등 비교 |
| NE | `0x21` | - | 0 | pop 2, push 1 | 비동등 비교 |
| LT | `0x22` | - | 0 | pop 2, push 1 | `<` |
| LE | `0x23` | - | 0 | pop 2, push 1 | `<=` |
| GT | `0x24` | - | 0 | pop 2, push 1 | `>` |
| GE | `0x25` | - | 0 | pop 2, push 1 | `>=` |
| BAND | `0x30` | - | 0 | pop 2, push 1 | 비트 AND |
| BOR | `0x31` | - | 0 | pop 2, push 1 | 비트 OR |
| BXOR | `0x32` | - | 0 | pop 2, push 1 | 비트 XOR |
| SHL | `0x33` | - | 0 | pop 2, push 1 | 좌시프트 |
| SHR | `0x34` | - | 0 | pop 2, push 1 | 우시프트 |
| NEW_TABLE | `0x40` | - | 0 | push 1 | 빈 테이블 생성 |
| GET_TABLE | `0x41` | - | 0 | pop 2, push 1 | `table[key]` 조회 |
| SET_TABLE | `0x42` | - | 0 | pop 3 | `table[key] = value` |
| JMP | `0x50` | `rel` | SLEB (현재 suru-bc: 고정 5바이트) | - | 상대 점프 |
| JMP_IF_FALSE | `0x51` | `rel` | SLEB (현재 suru-bc: 고정 5바이트) | pop 1 | falsey면 점프 |
| CALL | `0x60` | `arg_count, ret_count` | ULEB128 + ULEB128 | - | 함수 호출 |
| RETURN | `0x61` | `ret_count` | ULEB128 | pop `ret_count` | 함수 반환 |
| CLOSURE | `0x62` | `chunk_index` | ULEB128 | pop `chunk.upvalues`, push 1 | 클로저 생성 + upvalue 값 캡처 |
| GET_UPVALUE | `0x63` | `idx` | ULEB128 | push 1 | `closure->at(idx)` 조회 |
| SET_UPVALUE | `0x64` | `idx` | ULEB128 | pop 1 | `closure->at(idx)` 저장 |

## Opcode 상세

### CONST / GET_LOCAL / SET_LOCAL / GET_GLOBAL / SET_GLOBAL
- 인자: ULEB128 인덱스 1개
- 주의:
  - `GET_GLOBAL`/`SET_GLOBAL`의 key는 문자열 상수여야 한다.

### 산술/비교/비트/테이블
- 산술/비교/비트 연산은 스택에서 피연산자를 pop 후 결과를 push한다.
- 테이블 연산:
  - `GET_TABLE`: `key`, `table` 순 pop
  - `SET_TABLE`: `value`, `key`, `table` 순 pop

### JMP / JMP_IF_FALSE
- 인자: 상대 오프셋 `rel` (SLEB)
- 기준: 인자 디코드 후의 PC를 기준으로 `pc += rel`
- `JMP_IF_FALSE`는 조건값 1개를 pop하고 falsey일 때만 점프한다.

### CALL
- 인자: `arg_count`, `ret_count`
- callee는 closure여야 한다.
- 바이트코드 함수:
  - `arity` 기준 인자 정렬
  - 부족 인자는 `nil` 패딩
  - 초과 인자는 버림
- C 함수:
  - C 프레임 경로로 실행

### RETURN
- 인자: `ret_count`
- 현재 프레임에서 `ret_count`개를 pop해 반환값으로 만든다.
- caller의 `ret_slots`에 맞춰 truncate/pad(`nil`)한다.

### CLOSURE
- 인자: `chunk_index`
- 동작:
  1. 대상 청크로 closure 생성 (`len = chunk.upvalues`)
  2. 현재 스택에서 `chunk.upvalues`개를 pop해 upvalue에 저장
  3. 생성된 closure를 push
- 캡처 순서:
  - `v0, v1, v2`를 순서대로 push 후 `CLOSURE`를 실행하면
  - `up[0]=v0`, `up[1]=v1`, `up[2]=v2`가 된다.

### GET_UPVALUE / SET_UPVALUE
- 인자: `idx` (ULEB128)
- `GET_UPVALUE`: `closure->at(idx)`를 push
- `SET_UPVALUE`: pop한 값을 `closure->at(idx)`에 저장

## CALL / RETURN 규약 요약
- 인자 전달: 스택 기반
- 결과 전달: caller `ret_slots` 기준
- C 함수 내부에서도 `vm.call(...)` 재진입 가능

## 어셈블리 (`.sura`) 규칙
- 구조:
  - `.const`
  - `.chunk <name> <arity> <slots> <upvalues>`
- 점프는 라벨 기반
- 엔트리 포인트는 `main` chunk

## 주요 실패 조건
- `constant index out of bounds`
- `local slot out of bounds`
- `global key constant index out of bounds`
- `global key must be string`
- `closure chunk index out of bounds`
- `upvalue capture stack underflow`
- `upvalue index out of bounds`
- `call stack underflow`
- `jump target out of bounds`
- `return stack underflow`
- `unknown opcode`

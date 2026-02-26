# Suru VM Bytecode

## 목적과 범위
이 문서는 `libsuru-vm`의 현재 32비트 wordcode 형식을 정의한다.
기준 구현:
- `libsuru-vm/include/suru/vm/opcode.hpp`
- `libsuru-vm/src/vm_exec.cpp`
- `suru-bc/src/main.cpp`

## 기본 단위
- 명령어는 항상 32비트 1워드다.
- `CodeUnit::code_`는 `std::vector<uint32_t>`다.
- `Chunk.code_begin/code_end`는 바이트 오프셋이 아니라 워드 인덱스 범위다.

## 비트 포맷
- 공통: `op`는 상위 6비트 (`bits 31..26`)
- `ABC`: `op(6) | A(8) | B(9) | C(9)`
- `ABx`: `op(6) | A(8) | Bx(18)`
- `sAx`: `op(6) | sAx(26)` (2의 보수 signed)

## 인자 범위
- 레지스터 A: 8비트
- 레지스터 B/C: 9비트
- 인덱스 Bx: 18비트
- 점프 sAx: signed 26비트, 단위는 워드(명령어 개수)

## 호출 규약
- `CALL F argc retc`
  - callee: `R[F]`
  - args: `R[F+1] .. R[F+argc]`
  - 결과 저장: `R[F] .. R[F+retc-1]`
- 바이트코드 함수 인자 보정
  - 부족한 인자는 `nil`로 채움
  - 초과 인자는 무시

## 제어 흐름
- `JMP rel`: 무조건 상대 점프
- 조건 분기: `IF*` + `JMP` 조합
  - `IF*`는 조건이 거짓이면 다음 1워드를 건너뜀

예시:
```sura
LT 9 0 3
IFFALSEY 9
JMP loop_end
```

## Opcode 목록
| Opcode | 포맷 | 인자 | 동작 |
| --- | --- | --- | --- |
| MOVE | ABx | `A B` | `R[A] = R[B]` |
| LOADNIL / LOADTRUE / LOADFALSE | ABx | `A` | 상수 로드 |
| LOADK | ABx | `A K` | `R[A] = constants[K]` |
| GETGLOBAL | ABx | `A K` | `R[A] = globals[constants[K]]` |
| SETGLOBAL | ABx | `K A` | `globals[constants[K]] = R[A]` |
| ADD/SUB/MUL/DIV/IDIV/MOD/POW | ABC | `A B C` | 산술 연산 |
| NEG / NOT | ABx | `A B` | 단항 연산 |
| AND / OR | ABC | `A B C` | bool 논리 연산 |
| EQ/NE/LT/LE/GT/GE | ABC | `A B C` | 비교 결과(bool) 저장 |
| BAND/BOR/BXOR/SHL/SHR | ABC | `A B C` | 비트 연산 |
| NEWTABLE | ABx | `A` | `R[A] = {}` |
| GETTABLE | ABC | `A B C` | `R[A] = R[B][R[C]]` |
| SETTABLE | ABC | `A B C` | `R[A][R[B]] = R[C]` |
| JMP | sAx | `rel` | 무조건 상대 점프 |
| IFFALSEY / IFTRUTHY | ABx | `A` | 조건 거짓 시 다음 1워드 스킵 |
| IFEQ/IFNE/IFLT/IFLE/IFGT/IFGE | ABC | `B C` | 비교 거짓 시 다음 1워드 스킵 |
| CALL | ABC | `F argc retc` | 함수 호출 |
| RETURN | ABx | `A retc` | `R[A..]` 반환 |
| CLOSURE | ABx | `A chunk` | `R[A]`에 closure 생성 |
| GETUPVAL | ABx | `A U` | `R[A] = upvalue[U]` |
| SETUPVAL | ABx | `U A` | `upvalue[U] = R[A]` |

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

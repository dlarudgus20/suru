# Suru VM Bytecode

## 목적과 범위
이 문서는 `libsuru-vm`의 현재 32비트 워드 기반 바이트코드 형식을 정의한다.
기준 구현 파일:
- `libsuru-vm/include/suru/vm/opcode.hpp`
- `libsuru-vm/src/vm_exec.cpp`
- `suru-bc/src/main.cpp`

## 기본 단위
- 명령어는 항상 32비트 워드 1개다.
- 바이트코드 본문은 `CodeUnit::code_` (`std::vector<uint32_t>`)에 저장된다.
- `Chunk.code_begin` / `Chunk.code_end`는 워드 인덱스 범위를 뜻한다.

## 비트 포맷
- 공통: `op`는 상위 6비트 (`bits 31..26`)
- `ABC`: `op(6) | A(8) | B(9) | C(9)`
- `ABx`: `op(6) | A(8) | Bx(18)`
- `sAx`: `op(6) | sAx(26)` (2의 보수 signed)

## 레지스터와 호출 규약
- 레지스터는 현재 프레임의 `R[0..slots-1]` 범위를 사용한다.
- `CALL F argc retc`
  - callee: `R[F]`
  - 인자: `R[F+1] .. R[F+argc]`
  - 결과 저장: `R[F] .. R[F+retc-1]`
- 바이트코드 함수는 `chunk.arity` 기준으로 인자를 받는다.
  - 부족한 인자: `nil`로 채움
  - 초과한 인자: 무시

## 문자열/배열 연산 규약
- `CONCAT A B C`
  - `R[B]`, `R[C]`를 문자열로 변환해 이어 붙이고 `R[A]`에 저장한다.
  - 허용 타입: `string`, `number`, `boolean`
  - 변환 규칙: `boolean`은 `true`/`false` 문자열
  - 그 외 타입은 `TypeError("concat: expected string/number/boolean")`
- `LEN A B`
  - `R[B]`가 `string`이면 바이트 길이, `array`면 원소 수를 `number`로 `R[A]`에 저장한다.
  - 그 외 타입이면 `TypeError("len: expected string/array")`
- `NEWARRAY A B`
  - `R[B]`의 number 값을 길이로 사용해 새 배열을 만들고 `R[A]`에 저장한다.
  - 길이는 0 이상의 정수여야 하며, 아니면 `TypeError("newarray length must be non-negative integer")`
- `GETARRAY A B C`
  - `R[B]`는 array, `R[C]`는 인덱스(number)다.
  - 인덱스는 0-based이며 음수는 Python 규칙으로 변환한다.
  - 변환 후 범위를 벗어나면 `TypeError("array index out of bounds")`
- `SETARRAY A B C`
  - `R[A]`는 array, `R[B]`는 인덱스, `R[C]`는 저장 값이다.
  - 인덱스 규칙/오류는 `GETARRAY`와 동일하다.

## 업밸류 캡처 규약
`Chunk`는 `upvalue_infos`를 가진다. 각 항목은 `{ source, index }`다.
- `source = local`: 현재 프레임의 `R[index]`를 캡처
- `source = upvalue`: 현재 클로저의 `upvalue[index]`를 재캡처

`CLOSURE A chunk` 실행 시, 대상 chunk의 `upvalue_infos` 순서대로 업밸류가 채워진 클로저를 만들고 `R[A]`에 저장한다.

## 어셈블리(.sura) 포맷
- 섹션
  - `.const`
  - `.chunk <name> <arity> <slots>`
- chunk 내부 지시어
  - `.upvalue local <index>`
  - `.upvalue upvalue <index>`
- 라벨: `label:`
- 주석: `;` 이후 텍스트
- 엔트리 포인트: `main` chunk

예시:
```sura
.const
k1 = number 1

.chunk main 0 2
CLOSURE 0 inc
CALL 0 0 1
RETURN 0 1

.chunk inc 0 2
.upvalue local 1
GETUPVAL 0 0
LOADK 1 k1
ADD 0 0 1
SETUPVAL 0 0
RETURN 0 1
```

## Opcode 동작 요약
| Opcode | Format | Args | 동작 |
| --- | --- | --- | --- |
| MOVE | ABx | `A B` | `R[A] = R[B]` |
| LOADNIL / LOADTRUE / LOADFALSE | ABx | `A` | 상수 로드 |
| LOADK | ABx | `A K` | `R[A] = constants[K]` |
| GETGLOBAL | ABx | `A K` | `R[A] = globals[constants[K]]` |
| SETGLOBAL | ABx | `K A` | `globals[constants[K]] = R[A]` |
| ADD/SUB/MUL/DIV/IDIV/MOD/POW | ABC | `A B C` | 산술 연산 |
| CONCAT | ABC | `A B C` | 문자열 연결(`string/number/boolean` 허용) |
| NEG / NOT | ABx | `A B` | 단항 연산 |
| LEN | ABx | `A B` | 문자열 길이(바이트) |
| AND / OR | ABC | `A B C` | bool 논리 연산 |
| EQ/NE/LT/LE/GT/GE | ABC | `A B C` | 비교 결과(bool) 저장 |
| BAND/BOR/BXOR/SHL/SHR | ABC | `A B C` | 비트 연산 |
| NEWTABLE | ABx | `A` | `R[A] = {}` |
| NEWARRAY | ABx | `A B` | `R[A] = new array(len=R[B])` |
| GETTABLE | ABC | `A B C` | `R[A] = R[B][R[C]]` |
| SETTABLE | ABC | `A B C` | `R[A][R[B]] = R[C]` |
| GETARRAY | ABC | `A B C` | `R[A] = R[B][idx(R[C])]` |
| SETARRAY | ABC | `A B C` | `R[A][idx(R[B])] = R[C]` |
| JMP | sAx | `rel` | 상대 점프 |
| IFFALSY / IFTRUTHY | ABx | `A` | 조건 거짓일 때 다음 1워드 스킵 |
| IFEQ/IFNE/IFLT/IFLE/IFGT/IFGE | ABC | `B C` | 비교 거짓일 때 다음 1워드 스킵 |
| CALL | ABC | `F argc retc` | 함수 호출 |
| RETURN | ABx | `A retc` | `R[A..A+retc-1]` 반환 |
| CLOSURE | ABx | `A chunk` | 클로저 생성 및 저장 |
| GETUPVAL | ABx | `A U` | `R[A] = upvalue[U]` |
| SETUPVAL | ABx | `U A` | `upvalue[U] = R[A]` |


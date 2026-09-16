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
- `ABC`: `op(6) | i(1) | A(8) | B(8) | C(9)`
- `ABx`: `op(6) | i(1) | A(8) | Bx(17)` (`Bx = (B<<9)|C`)
- `sAx`: `op(6) | sAx(25)` (2의 보수 signed)

`i=1`일 때 일부 opcode는 `C` 또는 `Bx`를 immediate integer로 해석한다.
- `C` immediate: signed 9-bit (`-256..255`)
- `Bx` immediate: signed 17-bit (`-65536..65535`)

## 레지스터와 호출 규약
- 레지스터는 현재 프레임의 `R[0..slots-1]` 범위를 사용한다.
- `CALL F argc retc`
  - callee: `R[F]`
  - 인자: `R[F+1] .. R[F+argc]`
  - 결과 저장: `R[F] .. R[F+retc-1]`
- `CALL.v F retc`: argc 필드를 무시하고 `R[F+1]`부터 동적 `top` 직전까지 전달한다.
- `retc=511` (`0x1ff`): 결과를 전부 받는다. 그 외에는 고정 개수로 절단/`nil` 보충한다.
- CALL 반환 후 `top`은 결과 범위의 끝이다. 일반 레지스터 쓰기는 `top`을 바꾸지 않는다.
- `RETURN.v A`: `R[A]`부터 `top` 직전까지 반환한다.
- `.v`는 `i` 비트를 재사용한다. 생략된 필드는 assembler가 0으로 인코딩하며 VM은 무시한다.
- `top`은 절대 스택 인덱스이며 고정 레지스터 할당 끝과 별개다.
- open 값은 `slots` 바깥까지 확장될 수 있지만 일반 register operand의 범위를 확장하지 않는다.
- 바이트코드 함수는 `chunk.arity` 기준으로 인자를 받는다.
  - 부족한 인자: `nil`로 채움
  - 초과한 인자: 무시
  - 예외: `arity=255` (`@va`)는 모든 인자를 보존한다. 고정 인수 개수는 메타데이터에 없다.

## Vararg 명령

- `VARGPREP n` (`ABx`, A=n): 현재 `[base+1, top)`을 인수열로 해석해 재배치한다.
  - `n <= slots`, 부족한 고정 인수는 `nil`, `nextra=max(actual-n,0)`.
  - closure와 고정 인수를 새 레지스터 영역으로 복사하고 옛 고정 인수 슬롯은 `nil`로 만든다.
  - `base`는 새 closure 슬롯, `R[i]=stack[base+1+i]`, `top=base+1+n`.
  - hidden vararg는 `[base-nextra, base)`에 위치한다.
  - 첫 명령/1회 실행 제한이 없다. 숫자 arity chunk에서도 실행 가능하다.
  - 기존 hidden vararg를 자동 합치지 않는다. 반복 실행은 현재 인수열로 `nextra`를 대체한다.
  - 이전 슬롯의 upvalue는 이동하지 않는다. [VM 동작](vm-behavior.md)의 슬롯 보존 규칙을 따른다.
- `VARG A count` (`ABx`, A=목적지, Bx=count): extra를 고정 개수 복사하고 부족분은 `nil`; `top`은 유지한다.
- `VARG.v A`: extra를 전부 복사하고 `top=base+1+A+nextra`로 설정한다.
- prep 전에는 `nextra=0`이다. prep 자체는 필수가 아니며 `@va` 진입 인수는 `RETURN.v 0`으로 바로 반환할 수 있다.
- open list 및 0개 범위의 시작은 `A==slots`도 허용한다. 일반 레지스터 접근은 여전히 `A<slots`다.
- 인덱스/크기 연산은 `uint32_t` 범위를 검사하며, open 인수·결과 개수는 255개로 제한하지 않는다.

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
- `PUSHARRAYX A B count`
  - `R[A]`는 array이며 `R[B..B+count)`를 배열의 현재 끝에 순서대로 추가한다.
  - `count=0`이면 빈 범위이며 `B==slots`도 허용한다.
- `PUSHARRAYX.v A B`
  - `R[B]`부터 동적 `top` 직전까지 배열의 현재 끝에 추가한다.
  - open tail은 고정 `slots` 밖까지 이어질 수 있다. 명령 실행 후 `top`은 유지한다.

## Immediate 규약
- 어셈블리에서 immediate는 `#` 접두사로 표기한다.
  - 예: `ADD 0 1 #2`, `NEWARRAY 3 #8`, `GETARRAY 4 3 #-1`
- `i=1` immediate 활성 opcode
  - ABC: `ADD SUB MUL DIV IDIV MOD POW CONCAT EQ NE LT LE GT GE BAND BOR BXOR SHL SHR GETARRAY SETARRAY SETTABLE IFEQ IFNE IFLT IFLE IFGT IFGE SETARRAYI`
  - ABx: `LOAD NEWARRAY SETGLOBALK SETUPVAL SETGLOBAL`
- 그 외 opcode에서 `i`는 무시된다.
  - 단, `CALL`, `RETURN`, `VARG`, `PUSHARRAYX`에서는 `i`가 위의 `.v` 플래그다.

## 업밸류 캡처 규약
`Chunk`는 `upvalue_infos`를 가진다. 각 항목은 `{ source, index }`다.
- `source = local`: 현재 프레임의 `R[index]`를 캡처
- `source = upvalue`: 현재 클로저의 `upvalue[index]`를 재캡처

`CLOSURE A chunk` 실행 시, 대상 chunk의 `upvalue_infos` 순서대로 업밸류가 채워진 클로저를 만들고 `R[A]`에 저장한다.

`CLOSE A`는 현재 register bank의 `R[A]` 이상에 열린 upvalue를 모두 닫는다.
범위는 `[A, slots)`이며 `A == slots`는 빈 범위로 허용한다.
레지스터 값과 `top`은 바꾸지 않고, 이미 닫힌 upvalue에는 영향이 없다.
이전 `VARGPREP` register bank의 upvalue는 현재 범위에 포함되지 않으며 프레임 종료 시 닫힌다.

## 어셈블리(.sura) 포맷
- 섹션
  - `.const`
  - `.chunk <name> <arity> <slots>`
- arity는 `0..254` 또는 `@va`. 예: `.chunk forward @va 4`.
- `.const` 항목 타입은 `number`, `string`만 허용
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
표기 규약:
- `R[x]`: 현재 프레임 레지스터 `x`
- `RI[C]`: `i=1`이면 immediate `C`(signed 9-bit), 아니면 `R[C]`
- `RI[Bx]`: `i=1`이면 immediate `Bx`(signed 17-bit), 아니면 `R[Bx]`
- `I[B]`: `B` 필드를 immediate로 해석한다 (signed 8-bit)
- `K[k]`: 코드 유닛 상수 풀 인덱스 `k`
- `G[k]`: `global[k]`
- `U[u]`: 현재 클로저의 upvalue 슬롯 `u`

| Opcode | Format | Args | 동작 |
| --- | --- | --- | --- |
| LOAD | ABx | `A Bx` | `R[A] = RI[Bx]` |
| LOADNIL / LOADTRUE / LOADFALSE | ABx | `A Bx` | `R[A] = <literal>` |
| LOADK | ABx | `A Bx` | `R[A] = K[Bx]` |
| GETGLOBALK | ABx | `A Bx` | `R[Bx] = G[K[A]]` |
| SETGLOBALK | ABx | `A Bx` | `G[K[A]] = RI[Bx]` |
| GETGLOBAL | ABx | `A Bx` | `R[Bx] = G[R[A]]` |
| SETGLOBAL | ABx | `A Bx` | `G[R[A]] = RI[Bx]` |
| ADD/SUB/MUL/DIV/IDIV/MOD/POW | ABC | `A B C` | `R[A] = R[B] op RI[C]` |
| CONCAT | ABC | `A B C` | `R[A] = concat(R[B], RI[C])` |
| NEG / NOT | ABx | `A Bx` | `R[A] = op R[Bx]` |
| LEN | ABx | `A Bx` | `R[A] = len(R[Bx])` |
| AND / OR | ABC | `A B C` | `R[A] = R[B] op R[C]` |
| EQ/NE/LT/LE/GT/GE | ABC | `A B C` | `R[A] = cmp(R[B], RI[C])` |
| BAND/BOR/BXOR/SHL/SHR | ABC | `A B C` | `R[A] = bitop(R[B], RI[C])` |
| NEWTABLE | ABx | `A Bx` | `R[A] = {}` |
| NEWARRAY | ABx | `A Bx` | `R[A] = [nil; RI[Bx]]` |
| GETTABLE | ABC | `A B C` | `R[A] = R[B][R[C]]` |
| SETTABLE | ABC | `A B C` | `R[A][R[B]] = RI[C]` |
| GETARRAY | ABC | `A B C` | `R[A] = R[B][RI[C]]` |
| SETARRAY | ABC | `A B C` | `R[A][R[B]] = RI[C]` |
| GETARRAYI | ABC | `A B C` | `R[A] = R[C][I[B]]` |
| SETARRAYI | ABC | `A B C` | `R[A][I[B]] = RI[C]` |
| PUSHARRAYX | ABC | `A B count` | `R[B..B+count)`를 `R[A]` 끝에 추가 |
| PUSHARRAYX.v | ABC | `A B` | `R[B..top)`을 `R[A]` 끝에 추가 |
| JMP | sAx | `rel` | `pc = pc + rel` |
| IFFALSY / IFTRUTHY | ABx | `A Bx` | `if cond(R[A]) then pc = pc + 1` |
| IFEQ/IFNE/IFLT/IFLE/IFGT/IFGE | ABC | `A B C` | `if not cmp(R[B], RI[C]) then pc = pc + 1` |
| CALL | ABC | `F argc retc` | `call(R[F], argc, retc)` |
| CALL.v | ABC | `F retc` | `R[F+1]`부터 `top`까지 전달 |
| RETURN | ABx | `A Bx` | `return R[A..A+Bx-1]` |
| RETURN.v | ABx | `A` | `R[A]`부터 `top`까지 반환 |
| VARGPREP | ABx | `n` | 현재 인수열을 고정 n개와 extra로 재배치 |
| VARG | ABx | `A count` | extra count개 복사, 부족분 nil |
| VARG.v | ABx | `A` | extra 전부 복사, top 갱신 |
| CLOSURE | ABx | `A Bx` | `R[A] = closure(Bx)` |
| CLOSE | ABx | `A` | 현재 bank의 `[R[A], R[slots])` upvalue를 닫음 |
| GETUPVAL | ABx | `A Bx` | `R[Bx] = U[A]` |
| SETUPVAL | ABx | `A Bx` | `U[A] = RI[Bx]` |

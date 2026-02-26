# Suru VM Bytecode

## 목적
이 문서는 현재 `libsuru-vm`이 실행하는 바이트코드 규약을 정의한다.
구현 기준은 `libsuru-vm/include/suru/vm/opcode.hpp`, `libsuru-vm/src/vm_exec.cpp`이다.

## CodeUnit 포맷
`CodeUnit`은 3개 영역으로 구성된다.
- `constants_`: 상수 풀 (`number`, `string`, `nil`, `boolean` 등 `Value`)
- `opcodes_`: opcode + operand 바이트 스트림
- `chunks_`: 함수 단위 메타데이터 배열

`Chunk` 필드:
- `name`: 청크 이름 (`main` 진입점)
- `code_begin`: `opcodes_` 내 시작 오프셋
- `code_end`: `opcodes_` 내 끝 오프셋(배타)
- `max_slots`: 프레임 로컬 슬롯 수
- `upvalue_count`: 클로저 생성 시 업밸류 개수

## Operand 인코딩
- opcode: 1 byte (`Op` enum 값)
- 무부호 operand 인덱스: ULEB128
- 점프 상대 오프셋: SLEB128

## 실행 메모리 모델

### value stack
- 산술/비교/테이블 연산의 피연산자와 결과를 push/pop한다.
- 호출 시 callee와 인자가 같은 스택에 배치된다.

### frame base / local slot
- 각 `CallFrame`은 `base`를 가진다.
- 로컬 슬롯 인덱스 `i`는 실제 주소 `v_stack_[base + i]`를 뜻한다.
- `GET_LOCAL/SET_LOCAL`은 이 슬롯을 읽고 쓴다.

### max_slots
- 함수 호출 시 callee 프레임은 `max_slots` 크기로 확장된다.
- `arg_count > max_slots`면 호출 실패(`too many arguments for callee slots`).

### constants
- `CONST k`: 상수 풀 인덱스 `k` 값을 stack에 push한다.
- `GET_GLOBAL/SET_GLOBAL`의 키 상수는 반드시 string이어야 한다.

## CALL/RETURN 규약

### CALL `<arg_count> <ret_count>`
스택 top이 다음 구조라고 가정한다.
- `[..., callee, arg0, arg1, ..., argN-1]`

동작:
1. `callee`가 closure인지 검사
2. C closure면 callee 슬롯 제거 후 args를 연속 영역으로 맞춘 뒤 C 함수 실행
3. Suru closure면 args를 callee 시작 위치로 당기고 프레임 생성
4. 함수가 생산한 결과를 caller가 기대한 `ret_count`로 맞춤
- 부족하면 `nil` 패딩
- 많으면 앞에서 `ret_count`개만 사용

### RETURN `<count>`
- 현재 프레임에서 `count`개를 pop해 반환값 벡터를 만든다.
- 프레임 스택/슬롯을 정리한 뒤 caller로 전달한다.

## local / const / stack / slot 작동 예시

예: `CALL 2 1` 직전
- stack: `[..., callee, a, b]`

callee 진입 후(`max_slots = 4`):
- slot0 = `a`
- slot1 = `b`
- slot2 = `nil`
- slot3 = `nil`

`GET_LOCAL 0`은 slot0(`a`)를 push한다.
`SET_LOCAL 1`은 stack top을 pop해 slot1에 저장한다.

## Opcode 동작 레퍼런스

### 1) 스택/상수
- `POP`: stack top 1개 제거
- `NIL`: `nil` push
- `TRUE`: `true` push
- `FALSE`: `false` push
- `CONST <k>`: constants[k] push

### 2) 로컬/전역
- `GET_LOCAL <i>`: slot `base+i` 값을 push
- `SET_LOCAL <i>`: top pop 후 slot `base+i`에 저장
- `GET_GLOBAL <k>`: globals[constants[k]] push, 미존재면 `nil`
- `SET_GLOBAL <k>`: top pop 후 globals[constants[k]]에 저장

### 3) 산술/단항
- `ADD`, `SUB`, `MUL`, `DIV`, `IDIV`, `MOD`, `POW`
- `NEG`, `NOT`

공통 규칙:
- 산술 피연산자는 number여야 한다.
- `IDIV`는 `floor(lhs / rhs)`.
- `NOT`은 falsey(`nil` 또는 `false`)만 `true` 반환.

### 4) 비교
- `EQ`, `NE`
- `LT`, `LE`, `GT`, `GE`

공통 규칙:
- `EQ/NE`는 value 비교 함수 사용
- 크기 비교는 number 피연산자 요구

### 5) 비트
- `BAND`, `BOR`, `BXOR`, `SHL`, `SHR`

공통 규칙:
- 내부에서 number를 `int64`로 변환 후 연산
- 결과는 다시 number로 push

### 6) 테이블
- `NEW_TABLE`: 빈 table push
- `GET_TABLE`: `table, key` pop 후 `table[key]` push (없으면 `nil`)
- `SET_TABLE`: `table, key, value` pop 후 `table[key] = value`

### 7) 제어 흐름
- `JMP <rel>`: 현재 PC 기준 상대 점프
- `JMP_IF_FALSE <rel>`: cond pop, falsey면 점프

### 8) 함수/클로저
- `CLOSURE <chunk_index>`: 해당 chunk를 가리키는 closure 생성 후 push
- `CALL <arg_count> <ret_count>`: 호출 수행
- `RETURN <count>`: 현재 함수 반환

## 어셈블리(suru-bc) 대응
- `.const`: 상수 선언
- `.chunk <name> <max_slot> <upvalue_count>`: 청크 선언
- `label:`: 점프 라벨
- `main` 이름 청크가 진입점

## 실패 조건(대표)
- `constant index out of bounds`
- `local slot out of bounds`
- `global key must be string`
- `call stack underflow`
- `too many arguments for callee slots`
- `jump target out of bounds`
- `unknown opcode`
# Suru VM Runtime Overview

## 목적
이 문서는 `libsuru-vm` 런타임의 구성 요소와 실행 흐름을 설명한다.
Opcode 인코딩/명령별 스택 동작은 `docs/suru-vm/bytecode.md`에서 다룬다.

## 범위
- VM 상태 모델(`VM`, value stack, call frame)
- 코드 단위(`CodeUnit`, `Chunk`)
- 전역 환경/표준 라이브러리 연동
- 런타임 오류 발생 지점(요약)

## 핵심 구성
- `VM`: 힙 객체, 전역 테이블, 실행 스택을 소유한다.
- `suru::ir::CodeUnit`: VM과 독립된 소유형 number/string 상수와 chunk별 code를 담는다.
- runtime `CodeUnit`: materialized `Value` 상수, 평탄화된 `code_`, code 범위를 가진 `chunks_`를 담는다.
- `Chunk`: 실행 가능한 코드 구간과 프레임 슬롯 크기(`slots`)를 정의한다.
- `CallFrame`: PC, 원래 영역 시작, closure 슬롯 base, 논리 top, extra 개수, 반환 목적지와 기대 개수를 보관한다.

## 실행 수명주기
1. `VM::load_code_unit()`가 IR을 검증하고 runtime code unit과 엔트리 closure를 만든다.
2. 엔트리 closure를 value stack에 push하고 실행을 시작한다.
3. 엔트리 `Chunk`의 `slots` 크기만큼 프레임 슬롯을 확보한다.
4. Opcode fetch/dispatch loop를 실행한다.
5. `RETURN` 시 프레임 전체의 upvalue를 닫고 정리한다. RETURN 없는 code 끝 도달은 오류다.
6. 최상위 프레임 종료 시 실행이 완료된다.

## 값 모델
- `nil`
- `boolean`
- `number`
- `string`
- `array`
- `table`
- `closure` (Suru bytecode closure, C closure 포함)

## 전역 환경과 표준 함수
- 전역 변수는 VM의 global table에 저장된다.
- `libsuru-vm/src/lib.cpp`의 `load_libs(vm)`가 `print`, `to_number`, `str.*`, `table.*`, `math.*` 등을 등록한다.

## 오류 처리 요약
런타임은 다음 경우 예외를 던진다.
- 타입 불일치(예: 숫자 연산에 비숫자)
- 인덱스 범위 초과(상수/청크/로컬 슬롯)
- 호출 규약 위반(closure가 아닌 값 호출 등)
- 스택 언더플로/잘못된 점프 대상
- 언어/C API가 명시적으로 전달한 `RaisedError` payload

## 관련 문서
- 바이트코드 포맷/명령 상세: `docs/suru-vm/bytecode.md`
- VM 동작 설명: `docs/suru-vm/vm-behavior.md`
- 오류 분류 보충: `docs/suru-vm/runtime-errors.md`

# Suru VM Bytecode Spec (Current)

## 범위
이 문서는 `libsuru-vm`의 현재 구현을 기준으로 바이트코드 형식과 실행 규칙을 정의한다.

## 모듈 포맷
직렬화 순서(리틀 엔디언):
1. `magic` (4 bytes): `SURU`
2. `version` (`u32`)
3. `instruction_count` (`u32`)
4. instruction stream

## 명령어
- `0x00 HALT` : 실행 종료
- `0x01 CONST_I64 <i64>` : 스택에 정수 push
- `0x02 ADD_I64` : 상위 2개 값을 pop해 더한 뒤 push
- `0x03 PRINT_TOP` : 스택 top 값을 출력

`Instruction`는 `{ opcode, operand }` 구조를 사용하며, 현재 operand를 실제로 쓰는 명령은 `CONST_I64`만이다.

## 실행 규칙 (`suru::vm::execute`)
- VM은 `pc`를 0부터 증가시키며 순차 실행한다.
- `HALT`를 만나면 즉시 종료한다.
- 오류 조건:
  - `ADD_I64` 시 스택 원소 < 2: `stack underflow on ADD_I64`
  - `PRINT_TOP` 시 스택 비어 있음: `stack underflow on PRINT_TOP`
  - `max_stack` 초과 push: `stack overflow`
  - 알 수 없는 opcode: `unknown opcode`
- 종료 시 `ExecutionResult`에 `exit_code`, `error_message`, `final_stack`를 반환한다.

## 직렬화/역직렬화 API
- `serialize_module(...)` 실패 시 `false`와 에러 문자열 반환
- `deserialize_module(...)` 실패 시 `std::nullopt`와 에러 문자열 반환
- `disassemble(...)`는 사람이 읽을 수 있는 텍스트를 생성한다.

## 비목표(현재)
- 점프/분기, 함수 호출, 상수풀, 디버그 정보는 아직 정의하지 않는다.
- 프론트엔드(`libsuru`)와 바이트코드 생성 경로는 아직 연결하지 않는다.
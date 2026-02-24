# Suru Bytecode Specification (Draft)

## 모듈 포맷
직렬화된 바이트코드 모듈은 다음 순서로 저장한다.
1. magic: 4 bytes (`SURU`)
2. version: `u32` (little-endian)
3. instruction_count: `u32` (little-endian)
4. instruction stream

## 명령어 인코딩
각 명령어는 `opcode(u8)`로 시작한다. 일부 opcode는 뒤에 즉시값을 가진다.

| Opcode | Name      | Operand | 설명 |
|---|---|---|---|
| `0x00` | `HALT`     | 없음 | 실행 종료 |
| `0x01` | `CONST_I64`| `i64` | 정수 상수를 스택에 push |
| `0x02` | `ADD_I64`  | 없음 | 스택 상위 두 값을 pop 후 합산 push |
| `0x03` | `PRINT_TOP`| 없음 | 스택 최상단 값 출력 |

## VM 실행 규칙
- PC는 명령어 단위로 증가한다.
- `ADD_I64`는 최소 2개 스택 값이 필요하다.
- `PRINT_TOP`은 최소 1개 스택 값이 필요하다.
- `HALT`를 만나면 즉시 종료한다.

## 호환성 정책
- `version`이 지원 범위를 벗어나면 로더는 실패해야 한다.
- 새 opcode 추가 시 문서 표와 구현 enum을 동시에 갱신한다.

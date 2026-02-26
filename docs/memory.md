# Memory

## 용도
- 이 문서는 AI 작업 기억(최근 변경, 임시 결정, 후속 TODO)을 짧게 기록하는 곳이다.
- 상세 설계/명세는 각 문서에 두고, 여기에는 요약만 남긴다.
- 최신 항목이 위로 오도록 유지한다.

## 기록 규칙
- 새 항목을 추가할 때 기존 항목과 내용이 겹치면 중복으로 쌓지 말고, 기존 항목을 갱신하거나 통합한다.
- 같은 주제(예: 에러 체계, 문서 경로 변경)는 가장 최근 항목 하나만 유지하고 이전 항목은 요약/정리한다.

## 2026-02-27 #2
- VM 런타임 에러 분류가 정리되었다.
  - 제거: `NameError`, `CallError`, `RuntimeLimitError`
  - 사용: `TypeError`, `ApiError`, `TableError`, `InvalidCodeError`, `InvalidImageError`, `StackOverflowError`, `InternalError`
- `to_u32`/`to_u8` 보조 함수는 제거되었고, 스택 인덱스 `uint32_t` 범위 초과는 `StackOverflowError`로 처리한다.
- 문서 이름이 `docs/suru-vm/runtime-state-model.md`에서 `docs/suru-vm/vm-behavior.md`로 변경되었다.
- `docs/suru-vm/runtime-errors.md`와 `docs/suru-vm/vm-behavior.md`는 현재 구현 기준으로 갱신되었다.

## 2026-02-27 #1
- `libsuru-vm`에 upvalue 실행 경로가 추가되었고, C 함수에서도 `VM::getupvalue()`로 접근할 수 있다.
- C 함수 시그니처는 `void (*)(VM* vm)`로 단순화되었다.
- bool 전용 논리 opcode `AND`, `OR`가 추가되었다.
- 표준 함수 `div(a, b)`가 추가되었고 `(몫, 나머지)`를 다중 반환한다.
- 바이트코드 예제 테스트:
  - `tests/upvalue.sura`
  - `tests/div.sura`

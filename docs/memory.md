# Memory

## 용도
- 이 문서는 AI 작업 기억(최근 변경, 임시 결정, 후속 TODO)을 짧게 기록하는 곳이다.
- 상세 설계/명세는 각 문서에 두고, 여기에는 요약만 남긴다.
- 최신 항목이 위로 오도록 유지한다.

## 기록 규칙
- 새 항목을 추가할 때 기존 항목과 내용이 겹치면 중복으로 쌓지 말고, 기존 항목을 갱신하거나 통합한다.
- 같은 주제(예: 에러 체계, 문서 경로 변경)는 가장 최근 항목 하나만 유지하고 이전 항목은 요약/정리한다.

## 2026-09-16
- VM vararg/multret 구현: `.chunk name @va slots`, `CALL.v F retc`, `RETURN.v A`, `VARGPREP n`, `VARG A count`, `VARG.v A`, CALL retc=511.
- base는 closure 슬롯이며 반환 정보는 callee의 return_base/retc로 통합했다. frame_start는 반복 prep 이전 슬롯의 수명/정리 경계다.
- VARGPREP의 첫 명령/1회 실행 제한은 의도적으로 없다. 현재 인수열을 재해석하며, 기존 upvalue는 원래 슬롯에 남는다.
- SBC는 현재 형식만 읽고 쓰며 개발 중 만들어진 이전 이미지의 호환 변환은 제공하지 않는다.
- 범위 제외: CLOSE, SETARRAYX, source codegen.
- 검증: CTest 16개 통과, 새 VM 테스트 21개(인수 조합 117개 포함), CLI 회귀 5개. ASan/UBSan도 동일하게 통과했다. 실행 환경 제한으로 LeakSanitizer는 제외했다.

## 2026-02-27 #3
- `suru-bc`에 `-o <out.sbc> <in.sura>` 옵션이 추가되었다.
  - `-o`는 저장 전용이며 저장 후 실행하지 않는다.
  - 입력이 이미 `.sbc`일 때 `-o`를 주면 에러 처리한다.
- `suru-bc`는 `-o` 없이 입력 파일의 앞 4바이트를 검사해 `.sbc`를 자동 감지/로드한다.
- `.sbc` magic은 텍스트 혼동을 줄이기 위해 `\\0SBC`(bytes `00 53 42 43`)로 확정되었다.
- 어셈블리 `.const` 타입 제약이 강화되었다.
  - 허용: `number`, `string`
  - `nil/true/false` 상수 정의는 파싱 에러
- 관련 테스트가 추가되었다.
  - `suru_bc_emit_sbc_test`
  - `suru_bc_run_sbc_test`

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

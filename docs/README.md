# Documentation Index

## 구성
- `docs/suru/syntax.md`: Suru 문법 EBNF 정의
- `docs/suru/semantics-overview.md`: 언어 의미/동작 규약 개요
- `docs/suru-vm/runtime-overview.md`: VM 런타임 구성과 실행 흐름 개요
- `docs/suru-vm/bytecode.md`: 바이트코드 포맷, opcode 동작, 스택/슬롯 규약
- `docs/suru-vm/sbc-format.md`: `.sbc` 바이너리 컨테이너 포맷
- `docs/suru-vm/vm-behavior.md`: VM 동작 설명 문서
- `docs/suru-vm/runtime-errors.md`: 런타임 오류 분류/진단 보충 문서
- `docs/testing.md`: 테스트 실행/범위 가이드
- `docs/todo.md`: 후속 작업 TODO 목록
- `tests/upvalue.sura`: upvalue 캡처/읽기/쓰기 어셈블리 예제 테스트
- `tests/div.sura`: 표준 함수 `div(a, b)` 다중 반환(몫/나머지) 어셈블리 예제 테스트

## 현재 상태 요약
- 프론트엔드(`libsuru`)는 파싱 트리 출력 중심으로 동작한다.
- CLI(`suru`)는 REPL/파일 입력 파싱을 제공한다.
- VM(`libsuru-vm`)은 바이트코드 실행 경로를 제공한다.
- `suru-bc`는 `.sura`를 실행할 수 있고, `-o` 옵션으로 `.sbc`를 저장할 수 있다.
- `suru-bc`는 입력 앞 4바이트가 `\0SBC` magic이면 `.sbc`로 자동 로드해 실행한다.
- 어셈블리 `.const`는 `number`, `string`만 허용한다.
- 최근 VM 변경:
  - C 함수 시그니처가 `void (*)(VM* vm)`로 단순화되었다.
  - C 함수에서도 `VM::getupvalue()`로 upvalue에 접근할 수 있다.
  - bool 전용 논리 opcode `AND`/`OR`가 추가되었다.

# Documentation Index

## 구성
- `docs/suru/syntax.md`: Suru 문법 EBNF 정의
- `docs/suru/language-spec.md`: 언어 의미/동작 규약
- `docs/suru-vm/runtime-overview.md`: VM 런타임 구성과 실행 흐름 개요
- `docs/suru-vm/bytecode.md`: 바이트코드 포맷, opcode 동작, 스택/슬롯 규약
- `docs/suru-vm/runtime-state-model.md`: 런타임 상태 모델 보충 문서
- `docs/suru-vm/runtime-errors.md`: 런타임 오류 분류/진단 보충 문서

## 현재 상태 요약
- 프론트엔드(`libsuru`)는 파싱 트리 출력 중심으로 동작한다.
- CLI(`suru`)는 REPL/파일 입력 파싱을 제공한다.
- VM(`libsuru-vm`)은 바이트코드 실행 경로를 제공한다.
- 바이트코드 어셈블리 입력은 `suru-bc`로 실행할 수 있다.
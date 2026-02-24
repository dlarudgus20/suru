# Documentation Index

## 구성
- `docs/suru/syntax.md`: Suru 문법 EBNF 원문. 구현 기준 문법은 이 문서를 우선한다.
- `docs/suru/language-spec.md`: 언어 레벨 동작과 현재 구현 범위.
- `docs/suru-vm/bytecode-spec.md`: VM 바이트코드 형식과 실행 규칙.

## 현재 상태 요약
- 프론트엔드(`libsuru`)는 렉싱/파싱 및 파스트리 덤프를 제공한다.
- CLI(`suru`)는 REPL과 파일 입력 파싱을 지원한다.
- 백엔드(`libsuru-vm`)는 독립적인 바이트코드/VM 실험 모듈이다.
- 프론트엔드와 VM은 현재 직접 연결하지 않는다.

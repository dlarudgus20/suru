# Repository Guidelines

## 목적
이 저장소는 Lua-like 스크립트 언어 **Suru**를 학습용으로 구현한다. 현재 1차 목표는 소스 코드를 파싱해 트리(YAML 스타일)를 출력하는 것이다.

## 커뮤니케이션 규칙
- 기본 작업 언어는 **한국어**다.
- `/review` 같은 코드 리뷰 요청에도 **한국어로만** 설명한다.
- 변경 사유, 영향 범위, 테스트 결과를 짧고 명확하게 남긴다.

## 프로젝트 구조
- `libsuru/`: 프론트엔드 라이브러리 (`suru::front`)
  - `include/suru/front`: 공개 API (`token`, `lexer`, `parser`, `parse`, `dump`)
  - `src`: 렉서/파서/세션 파서/덤퍼 구현
  - `tests/front_smoke_test.cpp`
- `libsuru-vm/`: VM 라이브러리 (`suru::vm`) 및 바이트코드 구조
- `suru/`: CLI 실행 파일, REPL 및 파일 파싱 진입점
- `docs/`: 언어/바이트코드 명세 문서

## 빌드/테스트/실행
- 구성: `cmake -S . -B build`
- 빌드(Debug): `cmake --build build --config Debug`
- 테스트: `ctest --test-dir build -C Debug --output-on-failure`
- CLI 실행(Windows): `build\\suru\\Debug\\suru.exe`
  - `suru` 단독 실행: REPL
  - `suru <file.suru>`: 파일 파싱 트리 출력

## 코딩 스타일
- C++23 기준, 네임스페이스는 `suru::front`, `suru::vm` 사용.
- 헤더는 모듈 경계를 명확히 유지한다. (`parser.hpp`는 `parse.hpp`에 의존하지 않음)
- 파싱 루트 노드는 `Block`이다(이전 `Chunk` 사용 금지).

## 테스트 원칙
- 새 기능 추가 시 최소 1개 스모크 테스트를 함께 수정/추가한다.
- 파서 변경 시 성공 케이스와 실패/불완전 입력(`ParseStatus::Incomplete`)을 모두 확인한다.

## 커밋/PR 가이드
- 현재 히스토리는 초기 커밋만 존재한다. 앞으로는 `feat:`, `fix:`, `refactor:`, `docs:` 접두어를 권장한다.
- PR에는 목적, 주요 변경 파일, 수동 테스트 명령과 결과를 포함한다.

## Windows 환경 주의점
- 이 항목은 Windows에서 작업할 때 적용한다. Windows가 아니라면 무시한다.
- 작업 파일은 CRLF를 유지한다. 새로 만드는 파일도 전부 CRLF를 사용한다.
- UTF-8 without BOM 인코딩을 사용하며 인코딩이 깨지지 않게 조심한다.

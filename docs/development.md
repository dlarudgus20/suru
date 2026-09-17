# Development Guide

## 개발 환경
- 언어/표준: C++23
- 빌드 시스템: CMake
- 주요 타깃: `libsuru-ir`, `libsuru-vm`, `libsuru`, `suru`

## 자주 쓰는 명령
- 구성: `cmake -S . -B build`
- 빌드(Debug): `cmake --build build --config Debug`
- 테스트: `ctest --test-dir build -C Debug --output-on-failure`

Windows 실행 파일 예시:
- `build\\suru\\Debug\\suru.exe`

## 코드 경계 원칙
- 프론트엔드 API: `suru::front`
- IR API: `suru::ir`
- VM API: `suru::vm`
- `libsuru`는 `libsuru-ir`에 의존하지만 `libsuru-vm`에는 의존하지 않는다.
- `suru` 실행 파일이 compiler, IR, VM을 조합한다.

## 문서 기준
- 문법 정의: `docs/suru/syntax.md`
- 언어 동작: `docs/suru/semantics-overview.md`
- Compiler: `docs/suru/compiler.md`
- VM 바이트코드: `docs/suru-vm/bytecode.md`

## 텍스트 파일 규칙(Windows 기준)
- 줄바꿈: CRLF
- 인코딩: UTF-8 (BOM 없음)
- 최소한 `AGENTS.md`, `docs/` 문서는 위 규칙을 유지한다.

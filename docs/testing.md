# Testing Guide

## 목적
이 문서는 현재 저장소 테스트의 범위와 실행 방법, 테스트 추가 원칙을 정리한다.

## 테스트 목록
- `front_smoke_test` (`libsuru/tests/front_smoke_test.cpp`)
  - 파서 성공/실패/불완전 입력 상태를 확인한다.
- `cli_smoke_test` (`suru/tests/cli_smoke_test.cpp`)
  - CLI 실행 가능 여부를 확인한다.
- `vm_smoke_test` (`libsuru-vm/tests/vm_smoke_test.cpp`)
  - 기본 바이트코드 실행 결과를 확인한다.

## 실행 명령
- 전체 테스트:
  - `ctest --test-dir build -C Debug --output-on-failure`
- 특정 테스트:
  - `ctest --test-dir build -C Debug -R front_smoke_test --output-on-failure`

## 테스트 추가 원칙
- 기능 변경 시 최소 1개 테스트를 같이 수정/추가한다.
- 파서 변경은 아래 3가지를 분리 검증한다.
  - 정상 구문 파싱 성공
  - 문법 오류 진단 출력
  - 미완성 입력(`ParseStatus::Incomplete`) 처리
- CLI 메시지 형식을 바꾸면 `cli_smoke_test` 또는 별도 테스트를 반드시 갱신한다.

## 권장 절차
1. `cmake --build build --config Debug`
2. `ctest --test-dir build -C Debug --output-on-failure`
3. 실패 시 해당 테스트만 재실행해 원인을 좁힌다.
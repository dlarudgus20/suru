# Diagnostics Guide

## 목적
이 문서는 `suru` CLI가 출력하는 진단 정보 형식과 해석 방법을 정리한다.

## 기본 구조
진단 정보는 아래 형식으로 출력한다.
- parse 오류: `<path>:line:col: error: ...`
- runtime 오류: `<path>:line:col: runtime error: ...`

REPL 모드에서는 파일 경로가 `<repl>`로 출력된다.

## 상태별 의미
- `ParseStatus::Ok`: 현재 입력 조각이 완전한 구문
- `ParseStatus::Incomplete`: 입력이 아직 덜 닫힘(추가 입력 필요)
- `ParseStatus::Error`: 문법 오류 확정

REPL 모드가 아니라면 `Incomplete`는 `unexpected end of file` 진단 1개로 변환된다.

## 메시지 분류

### 1) 토큰/구문 기대 실패 (`expected ...`)
가장 많은 진단 유형이다. 특정 위치에서 필요한 토큰이 없을 때 발생한다.

주요 예시:
- `expected expression`: 표현식이 필요한 자리에서 시작 토큰이 아님
- `expected '='`: 할당/테이블 필드에서 `=` 누락
- `expected ')'`, `expected ']'`, `expected '}'`: 닫는 구분자 누락
- `expected 'end'`, `expected 'then'`, `expected 'do'`, `expected 'until'`: 블록 구조 키워드 누락
- `expected function name`, `expected parameter name`, `expected field name`, `expected local name`: 식별자 자리 오류
- `expected ','`, `expected '::'`, `expected '>'`: 구문 구분자 누락

### 2) 문장 형태 오류
- `expected assignment or function call statement`
  - statement 위치에서 유효한 문장 형태가 아님
- `expected variable in assignment`
  - `a, <expr> = ...`처럼 좌변에 변수 형태가 아닌 항목이 들어감

### 3) 입력 끝/잔여 토큰 오류
- `unexpected token after block`
  - 최상위 `Block`을 완성한 뒤에도 해석 불가능한 토큰이 남음

### 4) 불완전 입력 요약
- `unexpected end of file`
  - 파서가 `Incomplete`로 판정한 입력을 `parse()`가 단일 진단으로 요약한 값

## 원인 빠른 매핑
- 괄호/중괄호/대괄호/블록 키워드 누락: `expected ')'`, `expected '}'`, `expected 'end'` 등
- 잘못된 할당문: `expected '='`, `expected variable in assignment`

## 주의사항
- 파서는 첫 오류를 기준으로 진단을 1개만 적재한다.
- `ok() == false`이면 `ParseResult.tree`는 미완성 트리일 수 있다.

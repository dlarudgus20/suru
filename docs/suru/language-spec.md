# Suru Language Spec (Current)

## 범위
이 문서는 현재 코드베이스(`libsuru`, `suru`)에서 실제 동작하는 파서/REPL 동작을 정의한다.
정식 문법 정의(Ebnf)는 `docs/suru/syntax.md`를 기준으로 한다.

## 소스 입력
- 입력 단위는 UTF-8 문자열이다.
- 파서는 파일 경로가 아니라 문자열을 직접 받는다.
- BOM 검사는 하지 않는다.

## 파싱 모델
- 최상위 노드는 `Block`이다.
- 결과 타입은 `suru::front::ParseResult`.
  - `ok() == true`: `diagnostics`가 비어 있음
  - `ok() == false`: 진단 정보 포함, `tree`는 부분 트리일 수 있음
- 노드 구조: `kind`, `loc`, `attributes`, `nodes`, `lists`.

## 세션 파싱(REPL)
- `suru::front::ParserSession::parse_fragment()`는 입력 조각을 누적해 파싱한다.
- 반환 상태:
  - `ParseStatus::Ok`: 완전한 구문
  - `ParseStatus::Incomplete`: 추가 입력 필요
  - `ParseStatus::Error`: 문법 오류
- REPL은 `Incomplete`일 때 보조 프롬프트(`>>`)를 사용한다.

## CLI 동작
- `suru`: REPL 시작
- `suru <file>`: 파일을 읽어 파싱 트리를 출력
- 출력 포맷은 YAML 스타일 덤프(`suru::front::dump`)를 사용한다.

## 비목표(현재)
- 프론트엔드에서 바이트코드 생성/VM 실행은 아직 연결하지 않는다.
- 의미 분석, 타입 시스템, 최적화는 현재 범위 밖이다.
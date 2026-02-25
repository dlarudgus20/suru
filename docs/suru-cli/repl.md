# REPL Behavior

## 실행 모드
- `suru` 단독 실행: REPL 시작
- `suru <file>`: 파일 파싱 후 트리 출력

## 입력 처리
REPL은 `ParseContext`를 사용해 라인 단위 입력을 누적 파싱한다.
입력 라인마다 `parse(line + "\n", context)`를 호출한다.

## 상태 전이
- `ParseStatus::Ok`
  - 현재 누적 입력이 완전한 구문
  - 트리를 출력하고 다음 입력으로 진행
- `ParseStatus::Incomplete`
  - 입력이 아직 닫히지 않음
  - 추가 입력을 기다림
- `ParseStatus::Error`
  - 진단 메시지 출력
  - 입력 버퍼를 비우고 다음 입력으로 진행

## 프롬프트
- 기본 프롬프트: `> `
- 이어쓰기 프롬프트: `>> ` (`Incomplete` 상태에서 사용)

## REPL 종료
첫 줄 상태에서 아래 명령을 입력하면 종료한다.
- `.exit`
- `.quit`

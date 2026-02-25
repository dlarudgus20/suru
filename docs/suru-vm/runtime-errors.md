# Suru Runtime Errors and REPL

## 오류 분류
- `TypeError`: 연산 피연산자 타입 불일치
- `NameError`: 식별자 해석 실패
- `CallError`: 호출 불가능 값 호출
- `TableError`: 잘못된 키 접근/갱신
- `RuntimeLimitError`: 스택/리소스 제한 초과

## 진단 구조
런타임 진단은 다음 필드를 가진다.
- `category`: 오류 분류
- `message`: 사용자 메시지
- `location`(optional): 소스 위치

## 실패 정책
- 치명 오류 발생 시 현재 평가를 중단하고 `error` 상태 반환
- 복수 진단 수집은 선택 정책으로 두되, 기본은 첫 오류 우선

# Suru Runtime State Model

## 상태 구성
런타임 상태는 `(EnvStack, Store, CallStack, ControlState)`로 정의한다.
- `EnvStack`: 블록/함수 스코프 체인
- `Store`: 값 저장소(테이블/클로저 포함)
- `CallStack`: 함수 호출 프레임
- `ControlState`: normal, break, return, error

## 값 모델
지원 값 타입:
- `nil`
- `boolean`
- `number`
- `string`
- `function`
- `table`

## 스코프/바인딩
- `local` 선언은 현재 블록 스코프에 바인딩을 생성한다.
- 이름 조회는 안쪽 스코프에서 바깥 스코프 순으로 진행한다.
- 조회 실패 시 전역 환경을 확인한다.

## 호출 프레임
프레임은 다음 정보를 가진다.
- 함수 본문 참조
- 지역 환경 시작점
- 인자 바인딩 결과
- 반환 지점

## 테이블 상태 규칙
테이블 필드 종류:
- named field: `Name = exp`
- computed field: `[exp] = exp`
- array field: `exp`

배열 필드는 삽입 순서대로 `0, 1, 2, ...` 키를 할당한다.

## Suru Note
- 중복 키가 발생하면 마지막 대입값이 유효하다고 본다.

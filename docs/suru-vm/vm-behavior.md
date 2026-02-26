# Suru VM Behavior

## 개요
이 문서는 현재 `libsuru-vm` 구현의 동작을 설명한다.

## 상태 구성
VM은 아래 상태를 유지한다.
- `objects_`: 힙 객체(String/Table/Closure) 단일 연결 리스트
- `interned_strings_`: interned string 집합
- `code_units_`: 로드된 `CodeUnit` 소유 컨테이너
- `global_table_`: 전역 변수 테이블
- `v_stack_`: 값 스택(레지스터 저장소)
- `i_stack_`: 호출 프레임 스택

생성자에서 sentinel 프레임(`closure == nullptr`)을 `i_stack_`에 1개 넣는다.

## 값과 객체
값 종류는 `Nil`, `Boolean`, `Number`, `String`, `Table`, `Closure`다.
- `String`, `Table`, `Closure`는 힙 객체 포인터다.
- `Closure`는 두 종류다.
  - 바이트코드 클로저: `code != nullptr`
  - C 함수 클로저: `code == nullptr`, `cfunc != nullptr`

## Upvalue 모델
- upvalue는 값 복사가 아니라 참조 셀(`Upvalue`)로 표현된다.
- open 상태: `ptr`가 `v_stack_` 슬롯을 직접 가리킨다.
- closed 상태: 프레임 종료 시 `closed = *ptr`로 값을 옮기고 `ptr = &closed`로 전환된다.
- 같은 슬롯을 여러 클로저가 캡처하면 같은 `Upvalue` 셀을 공유한다.

## CallFrame
CallFrame의 각 필드는 다음 의미를 가진다.
- `closure`: 현재 실행 함수
- `pc`: 현재 워드 인덱스
- `base`: 현재 프레임의 레지스터 시작 오프셋(`v_stack_` 기준)
- `code_end`: 현재 청크 종료 워드 인덱스
- `ret_slots`: 나를 호출한 caller가 기대하는 반환 슬롯 수
- `call_dst`, `call_retc`: 내가 호출한 callee의 반환값이 복사되는 슬롯 위치와 수

## 호출/반환 규약
`CALL F argc retc`:
- callee는 `R[F]`
- 인자는 `R[F+1..F+argc]`
- 반환값은 caller의 `R[F..F+retc-1]`에 기록된다.

바이트코드 클로저 호출 시:
- callee 청크의 `arity`만큼 인자를 복사한다.
- 부족한 인자는 `nil`로 채우고, 초과 인자는 버린다.
- 프레임 레지스터 길이는 `slots`다.

C 클로저 호출 시:
- 인자는 프레임 base부터 연속으로 놓인다.
- C 함수는 `getlocal()`로 인자를 읽고 `push_value()`로 반환값을 넣는다.

반환 처리:
- 실제 반환 수와 `ret_slots`를 정규화해 caller 슬롯에 복사한다.
- 부족분은 `nil`로 채운다.

## API 동작 규약
- `getlocal(index)`: 현재 프레임의 local 값 조회. 범위를 벗어나면 `ApiError`.
- `getupvalue(index)`: 현재 클로저 upvalue 조회. sentinel 프레임/범위 초과는 `ApiError`.
- `push_value(value)`: 값 푸시. 내부 인덱스(`uint32_t`) 한계 초과 시 `InternalError`.
- `pop_value()`: 현재 프레임에서 pop. 프레임 경계 위반은 `InternalError`, API underflow는 `ApiError`.

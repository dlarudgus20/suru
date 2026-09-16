# Suru VM Behavior

## 상태와 프레임
VM은 힙 객체 목록, 문자열 intern 집합, CodeUnit 목록, 전역 테이블,
값 스택 `v_stack_`, 호출 프레임 스택 `i_stack_`, open upvalue 목록을 유지한다.
호출 스택의 첫 항목은 실제 함수 슬롯이 없는 sentinel이다.

| 필드 | 의미 |
| --- | --- |
| pc | 다음 명령의 워드 인덱스 |
| frame_start | 원래 호출용 closure 슬롯; 프레임의 정리 시작점 |
| base | 현재 closure 슬롯; VARGPREP으로 이동 가능 |
| top | 현재 인수/open 결과 범위의 끝 (절대 인덱스, exclusive) |
| nextra | 현재 hidden vararg 개수 |
| code_end | 실행 chunk의 끝 |
| return_base | 결과를 쓸 caller의 절대 스택 위치 |
| retc | 기대 결과 수; 511은 전체 결과 |

실제 프레임은 `stack[base]`에 closure를 보관한다.
`R[i] = stack[base+1+i]`이며 일반 레지스터는 chunk.slots 범위만 접근한다.
물리 스택 크기와 논리 top은 다르다. 예약된 레지스터나 과거 prep 영역은
top 뒤에도 남을 수 있으며, open 명령은 그 값을 암묵적으로 포함하지 않는다.

## 호출과 반환
CALL은 caller의 함수/인수를 물리 스택 끝에 복사한다. 새 프레임에서도
closure를 제거하지 않으며, caller의 원래 레지스터는 호출 중 유지된다.
결과 목적지는 `caller.base+1+F`이고 그 위치와 기대 개수는 callee에만 저장한다.

고정 함수 진입 시 부족한 인수는 nil, 초과 인수는 폐기하며 top은 고정 arity의 끝이다.
`@va` 함수는 실제 인수를 전부 보존하고 top을 그 끝으로 설정한다.
두 경우 모두 고정 레지스터 slots만큼 물리 공간을 확보한다.

`CALL.v F retc`는 R[F+1]부터 top까지 전달한다.
고정 retc는 결과를 절단하거나 nil로 보충하며, 511이면 전부 받는다.
반환 후 caller.top은 결과의 끝이다. retc=0은 결과 슬롯을 덮어쓰지 않는다.
일반 register 쓰기 및 고정 VARG는 top을 바꾸지 않는다.

`close_upvalues(frame_start)`로 현재 프레임 전체를 닫은 후 결과를 return_base에 쓴다.
목적지가 source보다 아래이므로 겹치는 범위도 정방향 복사한 뒤 callee 영역을 제거한다.
바이트코드 caller의 고정/과거 prep 영역은 유지한다.
C/API caller에는 호출용 함수 슬롯부터 결과를 남긴다.

## VARGPREP과 슬롯 수명
`VARGPREP n`은 현재 `[base+1, top)`을 인수열로 해석한다.
고정 n개는 새 closure 뒤로 복사하며 부족하면 nil, 기존 고정 슬롯은 nil로 바꾼다.
extra는 새 closure 직전의 `[base-nextra, base)`에 보존한다.
이후 top은 새 R0부터 n개 뒤다.

첫 명령 또는 1회 실행 제한은 없고, 고정 arity 함수에서도 사용 가능하다.
반복 실행은 이전 extra를 자동 병합하지 않고 현재 인수열로 nextra를 대체한다.
prep 전에는 nextra=0이다. `VARG`도 prep 없이 실행 가능하며 extra 0개로 동작한다.

기존 레지스터와 그 upvalue의 물리 슬롯을 함부로 회수하지 않는다.
새 closure/레지스터 영역은 항상 기존 물리 영역 뒤에 생성한다.
인수 끝과 물리 영역 끝이 같으면 extra는 원위치에 둔다.
예약 레지스터 또는 과거 값 때문에 두 끝이 다르면 extra 사본을 끝에 추가한 뒤
새 closure를 놓는다. 이 경우 옛 extra 슬롯 역시 프레임 종료까지 남는다.
이는 기존 upvalue를 통해 새 closure 슬롯이 덮어써지는 것을 막는다.

open upvalue는 포인터가 아닌 절대 슬롯 인덱스를 저장하므로 vector 재할당에 안전하다.
prep이 레지스터를 옮겨도 기존 upvalue는 옛 슬롯을 계속 참조한다.
옛 고정 슬롯은 nil이 되며, 새 고정 레지스터와 같은 local로 자동 연결되지 않는다.
프레임 종료/예외 해제 때 frame_start 이상을 모두 닫으므로 옛 슬롯의 upvalue도 안전하다.

## C/API 호출
- C 함수 인수는 base+1부터 시작하고 closure는 stack API에서 숨긴다.
- `getlocal(uint32_t)`와 `stack_top()`은 C/API 프레임의 보이는 값을 조회한다.
- `push_value()`로 결과를 추가한다. 기존처럼 C 함수 진입 당시 물리 끝 이후가 결과다.
- C 함수가 진입 당시 끝보다 스택을 줄였다면 남은 visible stack을 결과로 취급한다.
- `call(uint32_t argc, uint16_t retc)`는 재진입 가능하고 retc=511도 지원한다.
- `pop_value()`는 현재 closure 슬롯을 제거할 수 없고 제거되는 슬롯의 upvalue를 먼저 닫는다.
- API 호출에서 예외가 나면 그 호출의 프레임과 슬롯을 정리하고 예외를 다시 던진다.
- 인덱스 한계 초과는 StackOverflowError, 잘못된 bytecode 범위는 InvalidCodeError다.

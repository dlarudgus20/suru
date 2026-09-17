# Suru Expression Evaluation

## Truth and logical operators

`nil`과 `false`만 falsy다. `and`와 `or`는 단락 평가하며 boolean으로 변환하지 않고 선택된 operand 값을 반환한다.

```lua
nil or 10      -- 10
false and f()  -- false; f는 호출하지 않음
```

## Multiple values

함수 호출과 `...`만 여러 값을 만들 수 있다. 괄호로 묶은 호출은 한 값으로 조정한다.

Expression list에서는 마지막 식만 여러 값을 유지할 수 있다. 중간의 다중값 식은 첫 값 하나만 남기며, 고정 개수 문맥에서는 부족한 값을 `nil`로 채우고 초과 값을 버린다.

```lua
f(g(), 1)       -- g에서 한 값
f(1, g())       -- g의 모든 값
local a, b = g() -- 정확히 두 값
return 1, g()   -- 1 뒤에 g의 모든 값
```

배열 literal의 마지막 식에도 같은 규칙을 쓴다.

```lua
[1, g()] -- g의 모든 결과를 뒤에 추가
[g(), 1] -- g의 첫 결과만 사용
```

Table field의 value와 index key는 항상 한 값 문맥이다.

## Assignment order

다중 assignment는 다음 순서를 따른다.

1. RHS 식을 모두 평가하고 필요한 개수로 조정한다.
2. computed LHS의 object와 key를 왼쪽부터 평가한다.
3. LHS write를 왼쪽부터 수행한다.

따라서 `a, b = b, a`는 값을 교환하며, 앞쪽 write가 뒤쪽 LHS 주소 계산을 바꾸지 않는다.

## Operators and indexing

산술·비트·비교 연산은 VM의 해당 타입 규칙을 따른다. `#`는 string의 바이트 길이 또는 array 원소 수다. `~x`는 정수 비트 반전이다.

Array index는 0부터 시작하고 음수는 끝에서 센다. Table과 array의 source opcode는 모두 `GETINDEX`/`SETINDEX`이며 런타임 object 타입이 동작을 정한다.

Table key로 `nil`과 NaN은 허용하지 않는다. 존재하지 않는 유효한 table key를 읽으면 `nil`이다.

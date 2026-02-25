# Suru Semantics: Control Flow and Functions

## 제어 흐름
Normative Rule:
- `if`: 조건이 truthy인 첫 분기 블록 실행
- `while`: 조건이 truthy인 동안 반복
- `repeat`: 블록 1회 실행 후 조건 검사, truthy면 종료
- `break`: 가장 안쪽 루프 즉시 종료

Inference Rule:
```text
truthy(cond) = true  => execute(then_block)
truthy(cond) = false => try next branch
```

Example:
```lua
local x = 0
while x < 3 do
  x = x + 1
end
return x
```

Suru Note:
- 현재 코드는 파싱 중심이며, 실제 제어 흐름 실행은 런타임 단계에서 구현한다.

## 함수 정의와 호출
Normative Rule:
- `fn`은 함수 값을 만든다.
- 호출 시 인자를 새 호출 프레임의 파라미터에 바인딩한다.
- 본문 실행 후 반환값 목록을 호출자에게 돌려준다.

Inference Rule:
```text
call(f, args) => run(body, Env_f + params->args) => returns
```

Example:
```lua
local fn add2(x)
  return x, x + 1
end
return add2(2)
```

Suru Note:
- 함수 키워드는 `function`이 아니라 `fn`이다.

## 반환(다중 반환 포함)
Normative Rule:
- `return`은 값 없이 종료하거나, 하나 이상의 값을 반환할 수 있다.
- `return e1, e2, ...`는 값 목록을 순서대로 반환한다.

Inference Rule:
```text
return               => Ret([])
return e1, e2, ...   => Ret([v1, v2, ...])
```

Example:
```lua
return
return 1, 2, 3
```

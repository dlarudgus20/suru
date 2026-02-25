# Suru Semantics: Environments and Scope

## 스코프 모델
Normative Rule:
- `block`은 지역 스코프를 만든다.
- `local` 이름은 현재 블록과 하위 블록에서만 유효하다.
- 같은 이름의 `local`은 바깥 바인딩을 가린다(shadowing).

Inference Rule:
```text
Env' = extend(Env, x -> v)
Env', Store |- block => (Env'', Store')
```

Example:
```lua
local x = 1
do
  local x = 2
end
return x   -- 1
```

Suru Note:
- 현재 파서는 스코프 해석 결과를 AST에 직접 저장하지 않는다.

## 이름 해석
Normative Rule:
- 이름 조회는 가장 안쪽 스코프부터 바깥으로 진행한다.
- 지역에서 찾지 못하면 전역 환경 조회로 위임한다.

Inference Rule:
```text
lookup(Env, x) = nearest binding of x, else Global[x]
```

Example:
```lua
local a = 10
return a
```

Suru Note:
- `_ENV` 상세 동작은 런타임 문서에서 확장한다.

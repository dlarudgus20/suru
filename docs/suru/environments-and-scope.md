# Suru Environments and Scope

## Name resolution

Resolver는 이름을 가장 가까운 순서로 local, upvalue, global에 연결한다. 해석 결과는 AST를 복제하지 않고 `NodeId` 기반 semantic side table에 저장한다.

Nested function이 바깥 local을 사용하면 중간 함수까지 필요한 upvalue chain을 만든다. Closure 생성 시 upvalue source는 현재 함수의 local register 또는 upvalue index다.

## Local publication

일반 local initializer에서는 새 이름이 아직 보이지 않는다.

```lua
local x = 10
do
    local x = x + 1 -- 오른쪽 x는 바깥 x
end
```

반면 local function은 재귀를 위해 binding을 먼저 만든다.

```lua
local fn factorial(n)
    if n == 0 then return 1 end
    return n * factorial(n - 1)
end
```

## Lexical lifetime

각 block은 local scope다. Compiler는 함수 안의 local에 안정적인 register를 배정하고 block exit, `break`, `continue`, `return`에서 적절한 `CLOSE` 경계를 생성한다.

`repeat` 본문의 local은 `until` condition에서도 보인다. `continue`는 condition으로 이동하므로 repeat 본문 scope를 닫지 않고, 그보다 안쪽에서 빠져나온 scope만 닫는다.

전역 이름은 VM의 global table에 string key로 저장된다. `_ENV` binding이나 환경 교체 기능은 현재 없다.

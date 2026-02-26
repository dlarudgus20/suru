# Suru Semantics: Values and Types

## 값 도메인
Suru 값은 다음 집합으로 제한한다.
- `nil`
- `boolean` (`true`, `false`)
- `number`
- `string`
- `function`
- `table`

## 진릿값 규칙
Normative Rule:
- `false`와 `nil`만 falsy이다.
- 그 외 값은 모두 truthy이다.

Inference Rule:
```text
truthy(v) = false  if v in {false, nil}
truthy(v) = true   otherwise
```

Example:
```lua
if 0 then return 1 end
if nil then return 1 end
```

## 동등성(==, !=) 개요
Normative Rule:
- 같은 타입의 값끼리 비교한다.
- 숫자/문자열/불리언은 값 비교, 테이블/함수는 참조 동일성 비교를 기본으로 둔다.

Inference Rule:
```text
v1 == v2  <=>  same_type(v1, v2) and equal_by_kind(v1, v2)
v1 != v2  <=>  not (v1 == v2)
```

Example:
```lua
{} == {}      -- false
local t = {}
t == t        -- true
```

Suru Note:
- 메타메서드 기반 비교 확장은 현재 범위에서 제외한다.

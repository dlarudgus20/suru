# Suru Semantics: Expression Evaluation

## 일반 평가 규칙
Normative Rule:
- 식 `exp`는 현재 환경에서 값으로 평가된다.
- 우선순위/결합성은 [syntax.md](./syntax.md)의 연산자 규칙을 따른다.

Inference Rule:
```text
Env, Store |- exp => v
```

Example:
```lua
return 1 + 2 * 3   -- 7
```

## 논리 연산(`and`, `or`) 단락 평가
Normative Rule:
- `a and b`: `a`가 falsey면 `a`, 아니면 `b`
- `a or b`: `a`가 truthy면 `a`, 아니면 `b`

Inference Rule:
```text
a and b => (not truthy(a) ? a : b)
a or  b => (truthy(a) ? a : b)
```

Example:
```lua
return nil or "fallback"  -- "fallback"
```

Suru Note:
- short-circuit는 런타임 의미 규칙으로 고정한다.

## 단항/이항 연산
Normative Rule:
- 단항: `-`, `not`, `#`, `~`
- 이항: 산술/비트/비교/연결/논리 연산
- 피연산자 타입이 맞지 않으면 런타임 오류로 본다.

### 단항 연산 의미
| 연산자 | 의미 | 기대 피연산자 |
| --- | --- | --- |
| `-x` | 부호 반전 | `number` |
| `not x` | 논리 부정 (`truthy`/`falsey` 기준) | 모든 값 |
| `#x` | 길이 연산 | `string` 또는 `table` |
| `~x` | 비트 반전 | 정수로 해석 가능한 `number` |

### 이항 연산 의미
| 분류 | 연산자 | 의미 | 기대 피연산자 |
| --- | --- | --- | --- |
| 산술 | `+ - * / % ^^` | 기본 산술 연산 (`^^`는 거듭제곱) | `number` |
| 산술 | `//` | 내림 나눗셈(floor division) | `number` |
| 연결 | `..` | 문자열 연결 | `string` (또는 문자열로 변환 가능한 값) |
| 비트 | `& \| ^ << >>` | 비트 연산 (`^`는 XOR) | 정수로 해석 가능한 `number` |
| 비교 | `< <= > >=` | 대소 비교 | 같은 비교 가능한 타입 |
| 비교 | `== !=` | 동등/부정 동등 비교 | 모든 값 |
| 논리 | `and or` | 단락 평가 논리 연산 | 모든 값 |

Inference Rule:
```text
Env, Store |- e1 => v1   Env, Store |- e2 => v2
-----------------------------------------------
Env, Store |- e1 op e2 => apply(op, v1, v2)
```

Example:
```lua
return 2 ^^ 3, 1 ^ 2, 1 != 2, "a" .. "b"
```

Suru Note:
- 메타메서드 기반 연산자 오버로드는 현재 범위에서 제외한다.

## 테이블 식(`tableconstructor`)
Normative Rule:
- `{ ... }`는 새 테이블 값을 생성한다.
- 필드는 문법상 세 가지 형태를 가진다.
  - 계산 키 필드: `[exp] = exp`
  - 이름 필드: `Name = exp`
  - 배열 필드: `exp`
- 배열 필드는 순서대로 0부터 증가하는 정수 키에 저장된다.
- 필드 구분자는 `,` 또는 `;`이며, 마지막 구분자(trailing separator)를 허용한다.

Inference Rule:
```text
Env, Store |- field_i => (k_i, v_i)
------------------------------------
Env, Store |- {field_1, ... , field_n} => table(k_1=v_1, ... , k_n=v_n)
```

Example:
```lua
return {
  ["id"] = 1,
  name = "suru",
  10,
  20,
}
```

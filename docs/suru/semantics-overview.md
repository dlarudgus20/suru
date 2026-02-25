# Suru Semantics Overview

## 목적과 범위
이 문서는 Suru의 의미론(semantic)을 정의한다.

## 표기 규칙
각 섹션은 다음 순서로 기술한다.
1. Normative Rule
2. Inference Rule
3. Example
4. Suru Note

## 상태 모델
실행 상태는 `(Env, Store)`로 표기한다.
- `Env`: 이름 바인딩 환경
- `Store`: 값 저장소(추상 모델)

## 예시
Normative Rule: 식 `e`는 현재 환경에서 값 `v`로 평가된다.

Inference Rule:
```text
Env, Store |- e => v
```

Example:
```lua
local x = 1
return x + 2
```

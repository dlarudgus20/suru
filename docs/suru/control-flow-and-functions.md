# Suru Control Flow and Functions

## Branches and loops

`if`는 truthy인 첫 branch를 실행한다. `while`은 본문 전에, `repeat`는 본문 뒤에 조건을 검사한다.

Numeric `for`의 limit은 exclusive다. 초기값·limit·step은 loop 진입 때 한 번만 평가한다.

```lua
for i = 0, 3 do
    -- i: 0, 1, 2
end
```

양수 step은 `i < limit`, 음수 step은 `i > limit`인 동안 돈다. 상수 0 step은 compile error고 동적으로 얻은 0은 `RAISE`를 통한 runtime error다.

Generic `for`는 expression list를 iterator, state, control 세 값으로 조정한다. 매 반복마다 `iterator(state, control)`을 호출하고 첫 결과가 `nil`이면 종료하며, 첫 결과를 다음 control로 사용한다.

Loop에는 label을 붙일 수 있다.

```lua
outer: while ready do
    for i = 0, 10 do
        if done then break outer end
        if skip then continue outer end
    end
end
```

제어 이동은 빠져나가는 lexical scope의 captured local을 `CLOSE`한 뒤 수행한다.

## Functions and methods

`fn` expression은 closure를 만든다. `local fn name(...)`은 closure를 만들기 전에 local binding을 공개하므로 자기 재귀가 가능하다.

일반 `local name = initializer`는 initializer 평가가 끝난 뒤 새 binding을 공개한다.

Method 선언과 호출은 receiver를 정확히 한 번 평가한다.

```lua
fn object:add(x)
    self.value = self.value + x
end

object:add(2)
```

Method body의 첫 고정 인자는 암시적 `self`다.

## Varargs and returns

`...`는 현재 vararg function의 고정 parameter 뒤에 전달된 extra arguments다. Nested function은 바깥 `...`를 자동 capture하지 않는다.

Compiler는 vararg function의 prologue에 `VARGPREP <fixed-arity>`를 생성한다. `return`과 호출 인자는 multiple-value adjustment를 따른다.

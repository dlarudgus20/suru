# Suru Values and Types

## Value domain

Suru runtime 값은 다음 일곱 종류다.

- `nil`
- `boolean`
- `number` (`double`)
- interned `string`
- `array`
- `table`
- `function` closure

`suru::ir::Constant`에는 소유형 number와 string만 들어간다. Nil과 boolean은 전용 opcode로 생성한다.

## Equality and truth

같은 kind끼리 비교한다. Nil, boolean, number는 값 비교이고 interned string은 동일한 interned object로 비교된다. Array, table, function은 참조 동일성이다.

`nil`과 `false`만 falsy이며 0과 빈 string도 truthy다.

## Arrays

Array는 연속 원소를 소유한다. Index는 정수 number여야 하고 0-based다. 음수 index는 길이를 더해 해석한다. 범위를 벗어난 access는 `TypeError`다.

## Tables

Table은 모든 runtime 값을 key로 사용할 수 있지만 `nil`과 NaN은 거부한다. 없는 key의 read는 `nil`이다. Array와 table은 서로 다른 kind다.

## Functions and errors

Function은 bytecode closure 또는 C closure다. Bytecode closure는 captured upvalue를 소유한다.

`RAISE A`는 `R[A]`를 `RaisedError` payload로 그대로 보존한다. C 함수도 `VM::raise(Value)`로 같은 경로를 사용할 수 있다.

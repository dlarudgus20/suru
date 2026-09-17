# Suru Semantics Overview

현재 source 실행 pipeline은 다음과 같다.

```text
source
  -> typed AST
  -> resolver + NodeId semantic tables
  -> direct bytecode codegen
  -> suru::ir::CodeUnit
  -> VM materialization
  -> suru::vm::CodeUnit
  -> execute
```

Typed AST와 bytecode 사이에 별도 frontend IR/HIR은 없다. `suru::ir`은 VM pointer가 없는 소유형 bytecode image 계층이다.

핵심 의미 규칙은 다음과 같다.

- `nil`과 `false`만 falsy다.
- `and`/`or`는 단락 평가하고 operand 값을 반환한다.
- 함수 호출과 vararg는 Lua식 multiple-value adjustment를 따른다.
- assignment는 RHS, computed LHS, write 순서로 평가한다.
- local initializer는 선언 전 환경에서 평가하고 local function은 선언 후 closure를 만든다.
- array와 table은 별도 타입이며 index opcode는 통합되어 있다.
- lexical exit는 captured local의 `CLOSE`를 수행한다.
- 일반 goto, local attribute, implicit table array field는 없다.

세부 규칙은 같은 디렉터리의 syntax, expression, control-flow, environments, values 문서를 따른다.

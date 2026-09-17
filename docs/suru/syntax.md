# Suru Syntax

이 문서는 현재 프론트엔드가 구현하는 문법을 정의한다. 함수 키워드는 `fn`이며, 일반 label과 `goto`는 없다.

## Lexical

```ebnf
Name     ::= (letter | '_') {letter | digit | '_'}
Numeral  ::= digit {digit} ['.' digit {digit}] [('e' | 'E') ['+' | '-'] digit {digit}]
String   ::= '"' {char} '"' | "'" {char} "'"
Comment  ::= '--' {char - '\n'} ['\n']
```

## Statements

```ebnf
block ::= {stat [';']} [retstat [';']]

stat ::= varlist '=' explist
       | functioncall
       | 'break' [Name]
       | 'continue' [Name]
       | 'do' block 'end'
       | [Name ':'] 'while' exp 'do' block 'end'
       | [Name ':'] 'repeat' block 'until' exp
       | 'if' exp 'then' block {'elseif' exp 'then' block} ['else' block] 'end'
       | [Name ':'] 'for' Name '=' exp ',' exp [',' exp] 'do' block 'end'
       | [Name ':'] 'for' namelist 'in' explist 'do' block 'end'
       | 'fn' funcname funcbody
       | 'local' 'fn' Name funcbody
       | 'local' namelist ['=' explist]

retstat  ::= 'return' [explist]
varlist  ::= var {',' var}
var      ::= Name | prefixexp '[' exp ']' | prefixexp '.' Name
namelist ::= Name {',' Name}
funcname ::= Name {'.' Name} [':' Name]
```

Loop label은 `Name:` 바로 뒤의 `while`, `repeat`, `for`에만 붙는다. `break Name`과 `continue Name`은 같은 함수 안의 활성 loop label을 가리킨다.

다음 문법은 지원하지 않는다.

```text
goto name
::label::
local name <attribute>
```

## Expressions and calls

```ebnf
exp ::= 'nil' | 'false' | 'true' | Numeral | String | '...'
      | 'fn' funcbody | prefixexp | tableconstructor | arrayconstructor
      | exp binop exp | unop exp

prefixexp    ::= var | functioncall | '(' exp ')'
functioncall ::= prefixexp args | prefixexp ':' Name args
args         ::= '(' [explist] ')' | tableconstructor | String
funcbody     ::= '(' [parlist] ')' block 'end'
parlist      ::= namelist [',' '...'] | '...'
explist      ::= exp {',' exp}
```

## Arrays and tables

```ebnf
arrayconstructor ::= '[' [explist [',']] ']'

tableconstructor ::= '{' [fieldlist] '}'
fieldlist        ::= field {fieldsep field} [fieldsep]
field            ::= Name '=' exp
                   | literalkey '=' exp
                   | '(' exp ')' '=' exp
literalkey       ::= Numeral | String | 'true' | 'false'
fieldsep         ::= ',' | ';'
```

`[exp] = value` 형식의 table key와 `{1, 2}` 형식의 암시적 array field는 없다. 배열 값은 `[...]`로 만든다.

## Operators

높은 우선순위부터 다음 순서다.

```text
^^
unary: - not # ~
* / // %
+ -
..
<< >>
&
^
|
< <= > >= == !=
and
or
```

`^^`는 거듭제곱이고 `^`는 XOR이다.

# Suru Syntax (Lua 기반 EBNF 초안)

이 문서는 Suru 문법 설계를 시작하기 위한 **Lua 스타일 기준 문법**이다.
실제 구현 단계에서 Suru 전용 규칙으로 축약/확장한다.

## Lexical (요약)
```ebnf
Name       ::= ( letter | '_' ) { letter | digit | '_' }
Numeral    ::= digit { digit } [ '.' digit { digit } ] [ Exponent ]
Exponent   ::= ('e' | 'E') [ '+' | '-' ] digit { digit }
String     ::= '"' { char } '"' | '\'' { char } '\''
```

## Statements
```ebnf
block      ::= { stat [';'] } [ retstat [';'] ]

stat       ::= varlist '=' explist
             | functioncall
             | label
             | 'break'
             | 'goto' Name
             | 'do' block 'end'
             | 'while' exp 'do' block 'end'
             | 'repeat' block 'until' exp
             | 'if' exp 'then' block { 'elseif' exp 'then' block } [ 'else' block ] 'end'
             | 'for' Name '=' exp ',' exp [ ',' exp ] 'do' block 'end'
             | 'for' namelist 'in' explist 'do' block 'end'
             | 'function' funcname funcbody
             | 'local' 'function' Name funcbody
             | 'local' attnamelist [ '=' explist ]

retstat    ::= 'return' [ explist ]
label      ::= '::' Name '::'
funcname   ::= Name { '.' Name } [ ':' Name ]
varlist    ::= var { ',' var }
var        ::= Name | prefixexp '[' exp ']' | prefixexp '.' Name
namelist   ::= Name { ',' Name }
attnamelist::= Name [ attrib ] { ',' Name [ attrib ] }
attrib     ::= '<' Name '>'
explist    ::= exp { ',' exp }
```

## Expressions
```ebnf
exp        ::= 'nil'
             | 'false'
             | 'true'
             | Numeral
             | String
             | '...'
             | functiondef
             | prefixexp
             | tableconstructor
             | exp binop exp
             | unop exp

prefixexp  ::= var | functioncall | '(' exp ')'
functioncall ::= prefixexp args | prefixexp ':' Name args
args       ::= '(' [ explist ] ')' | tableconstructor | String
functiondef::= 'function' funcbody
funcbody   ::= '(' [ parlist ] ')' block 'end'
parlist    ::= namelist [ ',' '...' ] | '...'
```

## Tables
```ebnf
tableconstructor ::= '{' [ fieldlist ] '}'
fieldlist        ::= field { fieldsep field } [ fieldsep ]
field            ::= '[' exp ']' '=' exp
                   | Name '=' exp
                   | exp
fieldsep         ::= ',' | ';'
```

## Operators
```ebnf
binop      ::= '+' | '-' | '*' | '/' | '//' | '^' | '%'
             | '&' | '~' | '|' | '>>' | '<<'
             | '..'
             | '<' | '<=' | '>' | '>=' | '==' | '~='
             | 'and' | 'or'

unop       ::= '-' | 'not' | '#' | '~'
```

## 우선순위 (높음 -> 낮음)
```text
^,
unary(- not # ~),
* / // %,
+ -,
..,
<< >>,
&,
~,
|,
< <= > >= ~= ==,
and,
or
```

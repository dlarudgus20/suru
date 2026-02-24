# Suru Language Specification (Draft)

## 개요
Suru는 학습용 Lua-like 스크립트 언어다. 현재 스펙은 최소 실행 파이프라인 검증을 위한 초안이며, 문법과 바이트코드 모델을 점진적으로 확장한다.

## 소스 파일
- 파일 확장자 권장: `.suru`
- 인코딩: UTF-8

## 최소 문법 (v0)
```ebnf
program   = statement [";"] ;
statement = "print" expr | "return" expr ;
expr      = integer { "+" integer } ;
integer   = ["-"] digit { digit } ;
```

## 실행 의미
- `return <expr>`: 식을 계산하여 스택 최상단에 남기고 종료한다.
- `print <expr>`: 식을 계산한 뒤 최상단 값을 출력하고 종료한다.

## 오류 모델
- 컴파일 오류: 토큰/문법/정수 리터럴 파싱 실패
- 런타임 오류: 스택 언더플로/오버플로, 알 수 없는 opcode

## 버전 정책
- 본 문서는 `v0` 스캐폴드 기준이다.
- 문법 호환성 변화가 있으면 문서 버전을 증가시키고 변경 이력을 기록한다.

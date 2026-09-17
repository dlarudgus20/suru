# suru

Lua-like 스크립트 언어를 학습용으로 구현하는 프로젝트입니다.

## 구성
- `libsuru-ir`: VM 독립 IR, assembler/disassembler, SBC 입출력 (`suru::ir`)
- `libsuru-vm`: IR image를 로드하고 실행하는 VM (`suru::vm`)
- `libsuru`: lexer, typed AST, parser, resolver, compiler (`suru::front`)
- `suru`: source·assembly·SBC 실행과 AST·assembly·SBC 출력 CLI

## 빌드
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 실행
```bash
# REPL
./build/suru/suru

# source 컴파일·실행
./build/suru/suru <file.suru>
```

## 빠른 실행 예시
```bash
./build/suru/Debug/suru.exe tests/div.sura
./build/suru/Debug/suru.exe --sbc -o build/tests_div.sbc tests/div.sura
./build/suru/Debug/suru.exe build/tests_div.sbc
./build/suru/Debug/suru.exe tests/upvalue.sura
./build/suru/Debug/suru.exe --ir script.suru
./build/suru/Debug/suru.exe --disas build/tests_div.sbc
./build/suru/Debug/suru.exe --in=ir program.txt
ctest --test-dir build -C Debug --output-on-failure
```

입력 형식은 `--in=src|ir|sbc`로 지정합니다. 생략하면 `.sura`는 assembly, `.sbc`는 SBC, 나머지는 source로 처리합니다(확장자는 대소문자를 구분하지 않습니다). `--ir`은 assembly 텍스트를 출력하고, `--sbc`는 `-o`로 binary 파일을 저장하며, `--disas`는 SBC를 디스어셈블합니다. `--sbc`와 `--disas`는 REPL을 지원하지 않습니다.

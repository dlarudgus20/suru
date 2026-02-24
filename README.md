# suru

Lua-like 스크립트 언어를 학습용으로 구현하는 프로젝트입니다.

## 구성
- `libsuru-vm`: VM/바이트코드 백엔드(현재 프론트/CLI와 분리)
- `libsuru`: 프론트엔드 파서 라이브러리 (`suru::front`)
- `suru`: CLI 실행파일 (소스 파싱 후 구문 트리 YAML 출력)

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

# 파일 파싱
./build/suru/suru <file.suru>
```

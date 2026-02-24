# suru

학습용 Lua-like 스크립트 언어 프로젝트 스캐폴드.

## 구성
- `libsuru-vm`: 바이트코드 타입/로더/실행기
- `libsuru`: 프론트엔드 컴파일러
- `suru`: CLI (`run`, `compile`, `disasm`)

## 빌드
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

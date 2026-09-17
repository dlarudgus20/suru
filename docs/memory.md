# Memory

## 용도
- 이 문서는 AI 작업 기억(최근 변경, 임시 결정, 후속 TODO)을 짧게 기록하는 곳이다.
- 상세 설계/명세는 각 문서에 두고, 여기에는 요약만 남긴다.
- 최신 항목이 위로 오도록 유지한다.

## 기록 규칙
- 새 항목을 추가할 때 기존 항목과 내용이 겹치면 중복으로 쌓지 말고, 기존 항목을 갱신하거나 통합한다.
- 같은 주제(예: 에러 체계, 문서 경로 변경)는 가장 최근 항목 하나만 유지하고 이전 항목은 요약/정리한다.

## 2026-09-17
- 후속 리뷰: 미완성 문자열을 UnterminatedString token으로 구분하고 유효한 문자열 위치에서는 Incomplete로 처리해 REPL 버퍼 손실을 해결했다. 양쪽 따옴표, fragment 복구/범위, 끝의 backslash, 실제 구문 오류 구분, 실행/AST/IR REPL 및 batch 진단을 검증했고 Debug 빌드와 CTest 19개가 모두 통과했다. 공백·특수문자 chunk 이름의 assembly 왕복 실패와 괄호/단항 부호로 감싼 상수 0 step의 정적 검사 누락은 사용자 결정으로 known issue에 남기고 당장 수정하지 않는다.
- CLI 통합: `suru-bc` 소스/빌드 타깃을 제거하고 assembly/SBC 실행 및 테스트를 `suru`로 이전했다. 명시한 `--in=src|ir|sbc`가 우선하며 생략 시 확장자로 `.sura`는 IR, `.sbc`는 SBC, 나머지는 source를 선택한다. 확장자는 대소문자를 무시하며 content 추측은 없다.
- 출력 정책: `--ir`은 src/IR의 assembly text, `--sbc`는 src/IR의 binary 파일(`-o` 필수), `--disas`는 SBC의 assembly text다. Source 실행/AST/IR REPL만 유지하며 binary stdin/stdout은 거부한다. Source/IR batch stdin은 허용한다.
- 프론트엔드·IR 수정: `<main>` 및 `outer@0::inner@2` 이름과 고유성 검증, 함수별 loop/block-base 상태 분리, SBC upvalue count u8 및 최대 255개 검증, token/AST half-open range와 YAML begin/end 출력을 구현했다.
- `ir::UpvalueCount`는 u8이며 vector 크기를 변환하기 전에 검증한다. VM의 `Closure::len`은 기존 `size_t`를 유지한다. SBC layout 변경으로 이전 이미지는 재생성해야 하며 호환 계층은 없다.
- 검증: CMake 재구성, Debug 빌드와 CTest 19개 전체 통과. 형식/옵션 조합, 확장자와 명시 형식 우선순위, stdin/REPL 제한, SBC·assembly 왕복, 중첩 loop 경계/CLOSE, upvalue 255/256 및 AST range를 포함한다. ASan·UBSan은 현재 환경에서 미실행이다.
- Windows 로컬 빌드: Visual Studio 2022 Community에 포함된 CMake 3.29.5와 MSVC 19.41로 `build` Debug 빌드 및 CTest 19개 전체 통과를 확인했다. 추가 도구 설치나 관리자 권한은 필요 없었다.
- Codex 실행 프로세스의 Path/PATH 중복 때문에 MSBuild가 실패하면 프로세스 환경에서 두 항목을 제거하고 Path 하나로 복원한다. 시스템 환경변수는 수정하지 않았다.
- GoogleTest v1.15.2는 CMake FetchContent로 자동 다운로드했다. 샌드박스의 GitHub 연결 제한 때문에 다운로드 단계만 승인받아 샌드박스 밖에서 실행했다.

## 2026-09-16
- Source frontend 구현: typed AST, NodeId semantic side table, local/upvalue/global resolver, direct `ir::CodeUnit` codegen을 추가했다. 별도 frontend IR은 없다.
- `libsuru-ir`은 VM 독립 상수/chunk, instruction codec, builder, assembler/disassembler, 무버전 `.sbc` I/O를 소유한다. Runtime loader가 이를 `vm::CodeUnit`으로 materialize한다.
- Source semantics는 multiple values/vararg, method self, array/table literal, 동시 assignment, numeric/generic for, loop label, lexical CLOSE를 포함한다. goto/local attribute/implicit table array field는 없다.
- Table/array access는 `GETINDEX`/`SETINDEX`로 통합했고 nil/NaN table key를 거부한다. `RAISE A`와 `VM::raise(Value)`는 `RaisedError` payload를 보존한다.
- `suru`는 source와 `.sbc` 실행, `--ast`, `--ir`, 각 모드 REPL 및 명시적 stdin `-`를 제공한다. `suru-bc`는 assembly/disassembly와 image 실행을 담당한다.
- VM vararg/multret 구현: `.chunk name @va slots`, `CALL.v F retc`, `RETURN.v A`, `VARGPREP n`, `VARG A count`, `VARG.v A`. assembler의 multret 표기는 `@vret`이고 C=511로 인코딩한다.
- base는 closure 슬롯이며 반환 정보는 callee의 return_base/retc로 통합했다. frame_start는 반복 prep 이전 슬롯의 수명/정리 경계다.
- VARGPREP의 첫 명령/1회 실행 제한은 의도적으로 없다. 현재 인수열을 재해석하며, 기존 upvalue는 원래 슬롯에 남는다.
- SBC는 현재 형식만 읽고 쓰며 개발 중 만들어진 이전 이미지의 호환 변환은 제공하지 않는다.
- `PUSHARRAYX A B count`와 `PUSHARRAYX.v A B`는 배열 끝에 고정/open 레지스터열을 추가하며 top은 유지한다.
- `CLOSE A`는 현재 register bank의 `[R[A], R[slots])`에 열린 upvalue를 닫는다. `A == slots`는 빈 범위다.
- 검증: CTest 19개가 일반 Debug와 ASan/UBSan에서 모두 통과했다. GTest 73개와 source/bytecode CLI 회귀 13개를 포함한다. LeakSanitizer는 환경 제약 때문에 제외했다.

## 2026-02-27 #3
- `suru-bc`에 `-o <out.sbc> <in.sura>` 옵션이 추가되었다.
  - `-o`는 저장 전용이며 저장 후 실행하지 않는다.
  - 입력이 이미 `.sbc`일 때 `-o`를 주면 에러 처리한다.
- `suru-bc`는 `-o` 없이 입력 파일의 앞 4바이트를 검사해 `.sbc`를 자동 감지/로드한다.
- `.sbc` magic은 텍스트 혼동을 줄이기 위해 `\\0SBC`(bytes `00 53 42 43`)로 확정되었다.
- 어셈블리 `.const` 타입 제약이 강화되었다.
  - 허용: `number`, `string`
  - `nil/true/false` 상수 정의는 파싱 에러
- 관련 테스트가 추가되었다.
  - `suru_bc_emit_sbc_test`
  - `suru_bc_run_sbc_test`

## 2026-02-27 #2
- VM 런타임 에러 분류가 정리되었다.
  - 제거: `NameError`, `CallError`, `RuntimeLimitError`
  - 사용: `TypeError`, `ApiError`, `TableError`, `InvalidCodeError`, `InvalidImageError`, `StackOverflowError`, `InternalError`
- `to_u32`/`to_u8` 보조 함수는 제거되었고, 스택 인덱스 `uint32_t` 범위 초과는 `StackOverflowError`로 처리한다.
- 문서 이름이 `docs/suru-vm/runtime-state-model.md`에서 `docs/suru-vm/vm-behavior.md`로 변경되었다.
- `docs/suru-vm/runtime-errors.md`와 `docs/suru-vm/vm-behavior.md`는 현재 구현 기준으로 갱신되었다.

## 2026-02-27 #1
- `libsuru-vm`에 upvalue 실행 경로가 추가되었고, C 함수에서도 `VM::getupvalue()`로 접근할 수 있다.
- C 함수 시그니처는 `void (*)(VM* vm)`로 단순화되었다.
- bool 전용 논리 opcode `AND`, `OR`가 추가되었다.
- 표준 함수 `div(a, b)`가 추가되었고 `(몫, 나머지)`를 다중 반환한다.
- 바이트코드 예제 테스트:
  - `tests/upvalue.sura`
  - `tests/div.sura`

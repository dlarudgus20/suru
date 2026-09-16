# Suru VM SBC Binary Format

## 목적과 범위
이 문서는 `suru-bc`가 생성하고 `libsuru-vm`이 로드할 수 있는 `.sbc` 바이너리 컨테이너 형식을 정의한다.
바이트코드 워드(`op/i/A/B/C`) 해석 자체는 [bytecode.md](/C:/Users/dlaru/source/repos/suru/docs/suru-vm/bytecode.md)를 따른다.

## 기본 규칙
- 엔디안: little-endian 고정
- 컨테이너 정수: `u8`, `u16`, `u32` 고정폭
- 실수: IEEE-754 `f64`
- 문자열: UTF-8 without BOM, `u32 byte_len + raw bytes`
- 상수 풀 타입: `number`, `string`만 허용

## 파일 레이아웃
1. `Header`
2. `ConstantPool`
3. `ChunkTable`
4. `CodeWords`

## Header
| 필드 | 타입 | 설명 |
| --- | --- | --- |
| magic | `u32` | bytes `00 53 42 43` (`"\0SBC"`) |
| version | `u32` | 포맷 버전 |
| const_count | `u32` | 상수 개수 |
| chunk_count | `u32` | 청크 개수 |
| code_word_count | `u32` | `u32 word` 개수 |
| entry_chunk_index | `u32` | 엔트리 청크 인덱스 |

## ConstantPool
각 항목은 `tag:u8 + payload` 형식.

- `tag=1` (`number`): `f64`
- `tag=2` (`string`): `u32 len + u8[len]`

`nil/bool/table/array/closure` 태그는 무효다.

## ChunkTable
청크마다 아래 구조를 순서대로 기록한다.

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| name_len | `u32` | 청크 이름 바이트 길이 |
| name | `u8[name_len]` | UTF-8 이름 |
| code_begin | `u32` | 코드 시작 워드 인덱스 |
| code_end | `u32` | 코드 끝 워드 인덱스(exclusive) |
| arity | `u8` | 고정 파라미터 수 0..254, 255는 @va |
| slots | `u8` | 레지스터 슬롯 개수 |
| upvalue_count | `u16` | 업밸류 정보 개수 |
| upvalues | 반복 | `source:u8, index:u8` |

`source`는 `0=Local`, `1=Upvalue`.

## CodeWords
`u32 word[code_word_count]`를 그대로 저장한다.
각 word 디코딩은 `bytecode.md`의 `ABC/ABx/sAx` 규약을 사용한다.

## 로더 검증 규칙
- `magic`/버전 불일치 시 로드 실패
- 섹션 길이와 실제 파일 길이가 맞지 않으면 실패
- `entry_chunk_index >= chunk_count`면 실패
- `code_begin > code_end` 또는 `code_end > code_word_count`면 실패
- 고정 arity에서 `arity > slots`면 실패 (`@va`는 예외)
- 상수 태그 미지원이면 실패
- 실패 카테고리는 `InvalidImageError` 권장

## 형식 검증
- `version`은 현재 값 1만 허용한다.
- 개발 중 만들어진 이전 `.sbc`에 대한 호환 변환은 제공하지 않는다.
- 형식이 바뀌면 현재 assembler로 `.sbc`를 다시 생성한다.

## 최소 예시 (개념)
- 상수 1개(`number 1.0`)
- 청크 1개(`main`)
- 코드 2워드 (`LOADK`, `RETURN`)

구현자는 위 레이아웃 순서대로 직렬화/역직렬화하면 `CodeUnit { constants_, chunks_, code_ }`로 1:1 복원할 수 있다.

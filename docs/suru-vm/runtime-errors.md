# Suru Runtime Errors

## 오류 분류
- `TypeError`: 타입이 맞지 않는 연산/변환
- `ApiError`: VM C++ API 사용 오류
- `TableError`: 테이블 키/값 접근 또는 갱신 실패
- `InvalidCodeError`: 바이트코드 자체가 유효하지 않음(레지스터/점프/상수 인덱스 등)
- `InvalidImageError`: `CodeUnit`/`Chunk` 메타데이터가 유효하지 않음
- `StackOverflowError`: VM 내부 인덱스(`uint32_t`) 범위를 넘는 스택/프레임 크기
- `InternalError`: VM 내부 불변식 위반 또는 비정상 상태

## 진단 구조
모든 런타임 오류는 다음 정보를 가진다.
- `category`: 오류 카테고리
- `message`: 사람이 읽는 설명 메시지
- `location`: 현재 미지원(소스/바이트코드 매핑 추가 시 도입 예정)

## 처리 원칙
- 치명적 런타임 오류 발생 시 현재 실행을 중단하고 즉시 실패 처리한다.
- `suru-bc`는 `runtime error [Category]: message` 형식으로 출력한다.

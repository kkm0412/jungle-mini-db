# 고정 길이 CSV + B+Tree 인덱스 적용 계획

## Summary

- 현재 C11 기반 미니 DB 구조는 유지하고, 외부 빌드 연동 없이 `habedi/bptree`의 단일 헤더 `bptree.h`를 `third_party/`에 vendoring한다.
- 물리적 row는 항상 `64 bytes`로 저장한다. 이 크기는 줄바꿈 `\n`까지 포함한다.
- 각 CSV row의 마지막 필드는 내부 padding 컬럼으로 사용한다. 사용자 입력/출력에서는 숨기고, 파일에만 저장한다.
- B+Tree key는 첫 번째 컬럼 `id`이고, leaf value는 `{table_name}:{offset/row_size}` 형식의 row location으로 저장한다.

## Key Changes

- `mini_db.h`에 `ROW_SIZE = 64`, padding 관련 상수, `WHERE id` 조건을 담을 select plan 필드, `RowLocation` 타입을 추가한다.
- `parser.c`는 기존 `select * from table;`를 유지하고, `select * from table where id = 101;`를 추가 지원한다.
- `executor.c`는 CSV 줄 단위 읽기/쓰기 대신 64-byte fixed row 단위로 처리한다.
- B+Tree wrapper 모듈을 새로 추가해 라이브러리 타입을 외부로 노출하지 않는다.
- `data/users.csv`, `data/posts.csv`는 기존 데이터를 64-byte row 형식으로 한 번 변환한다. 런타임 자동 마이그레이션 코드는 추가하지 않는다.

## Data Format And Behavior

- row encoding은 `logical_value1,logical_value2,` 뒤에 `_` padding을 채워 총 63 bytes를 만들고 마지막 1 byte는 `\n`으로 둔다.
- SELECT 출력은 기존처럼 논리 컬럼만 보여준다. padding 컬럼은 컬럼명에도 출력하지 않고 row 출력에서도 제외한다.
- `select * from users where id = 101;`는 B+Tree에서 `id -> RowLocation`을 찾고, `fseek(table_path, row_index * 64, SEEK_SET)` 후 정확히 64 bytes만 읽는다.
- `select * from users;`는 파일 크기 기준으로 `0, 64, 128...` offset을 순차 스캔한다.
- INSERT는 먼저 id 중복 여부를 B+Tree로 확인한 뒤 파일 끝에 fixed row를 append한다. 중복 id이면 파일을 쓰지 않고 에러를 출력한다.
- logical row가 padding 포함 64 bytes에 들어가지 않으면 INSERT를 거부하고 `row size를 초과했습니다`를 출력한다.

## Test Plan

- `cmake --build cmake-build-debug`로 기존 빌드가 유지되는지 확인한다.
- `select * from users;`가 padding 없이 기존 논리 컬럼과 row만 출력하는지 확인한다.
- `select * from users where id = 101;`가 B+Tree 경로로 단일 row를 출력하는지 확인한다.
- 없는 id 검색은 에러가 아니라 컬럼명만 출력하는 빈 결과로 처리한다.
- INSERT 후 `data/*.csv` 파일 크기가 항상 `64`의 배수인지 확인한다.
- INSERT한 id를 즉시 `WHERE id`로 조회해 index 갱신과 offset 접근이 맞는지 확인한다.
- 중복 id INSERT, 64 bytes 초과 row INSERT, 잘못된 `where` 문법을 각각 거부하는지 확인한다.

## Assumptions

- `row_size = 64 bytes`는 줄바꿈까지 포함한 실제 파일 레코드 간격이다.
- B+Tree index는 파일에 별도 저장하지 않고, 프로그램 실행 중 메모리에 유지한다.
- 기존처럼 `id`는 INSERT 입력에 포함된다. 자동 id 발급은 이번 최소 변경 범위에 포함하지 않는다.
- padding 문자는 `_`로 고정한다.

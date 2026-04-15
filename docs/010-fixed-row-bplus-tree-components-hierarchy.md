# 고정 길이 Row + B+Tree 적용 후 SQL 처리기 구성 계층화

이 문서는 `docs/002-core-components-hierarchy.md`의 SQL 처리기 계층 구조에 `docs/009-fixed-row-bplus-tree-plan.md`의 고정 길이 row 저장 방식과 B+Tree 인덱스 설계를 추가했을 때의 전체 구조를 정리한다.

핵심 방향은 기존 REPL, 파서, 실행기 흐름은 유지하고, 파일 저장 계층을 64-byte fixed row 기반으로 바꾸며, `WHERE id = ?` 조회만 B+Tree 인덱스 경로로 분기하는 것이다.

## 1. 프로그램 시작 / 인덱스 준비

REPL이 SQL 입력을 받기 전에 테이블 파일과 인덱스 구조를 사용할 수 있는 상태로 준비하는 영역이다. 기존 구조에서는 명시적인 시작 단계가 거의 없었지만, B+Tree 인덱스를 사용하려면 시작 시점에 인덱스 초기화와 재구축 단계가 필요하다.

### 1.1 테이블 메타데이터 확인

고정된 테이블 이름, 논리 컬럼 목록, CSV 파일 경로, fixed row 크기 정보를 확인한다.

예시:

```text
users -> data/users.csv, row_size = 64
posts -> data/posts.csv, row_size = 64
```

### 1.2 B+Tree wrapper 초기화

`db_index_init_all()`을 호출해 테이블별 B+Tree 핸들을 준비한다. 실행기와 파서는 외부 라이브러리의 `bplus_tree_*` API를 직접 사용하지 않고, 항상 `db_index.c/.h` wrapper를 통해서만 인덱스에 접근한다.

### 1.3 CSV 기반 인덱스 재구축

프로그램 시작 시 각 CSV 파일을 64-byte 단위로 순차 스캔한다. 각 row의 첫 번째 컬럼인 `id`와 해당 row의 byte offset을 읽어 B+Tree에 등록한다.

예시:

```text
offset 0   -> id 1
offset 64  -> id 2
offset 128 -> id 3
```

## 2. REPL SQL 입력 처리

사용자가 터미널에서 SQL을 한 줄씩 입력하고, 입력된 SQL을 파싱과 실행 단계로 넘기는 영역이다. 이 계층은 기존 구조와 거의 동일하게 유지된다.

### 2.1 프롬프트 출력

사용자가 SQL을 입력할 수 있도록 REPL 프롬프트를 출력한다.

예시:

```text
mini-db>
```

### 2.2 SQL 한 줄 읽기

표준 입력에서 SQL 문자열 한 줄을 읽고, 앞뒤 공백과 개행을 제거한다.

### 2.3 특수 명령 처리

특수 명령은 마침표(`.`)로 시작한다. 현재 지원되는 명령은 `.exit`이며, 입력값이 `.exit`이면 인덱스 자원을 정리한 뒤 REPL을 종료한다.

## 3. SQL 파싱

입력된 SQL을 단순한 규칙으로 분석해 실행에 필요한 계획으로 바꾸는 영역이다. 기존 `SELECT`, `INSERT` 파싱에 `WHERE id = ?` 조건 파싱이 추가된다.

### 3.1 SQL 타입 판별

SQL이 세미콜론(`;`)으로 끝나는지 먼저 확인한 뒤, 문장이 `SELECT`인지 `INSERT`인지 구분한다.

### 3.2 SELECT 전체 조회 파싱

`select * from 테이블;` 형태에서 `select`, `*`, `from`, 테이블 이름, `;`, 입력 끝을 순서대로 확인한다. 이 형태는 인덱스를 사용하지 않고 전체 row를 순차 스캔하는 실행 계획이 된다.

예시:

```sql
select * from users;
```

### 3.3 SELECT id 조건 조회 파싱

`select * from 테이블 where id = 값;` 형태에서 `where`, `id`, `=`, 정수 값을 추가로 확인한다. 이 형태는 B+Tree 인덱스를 사용하는 실행 계획이 된다.

예시:

```sql
select * from users where id = 101;
```

### 3.4 INSERT 테이블명과 값 목록 추출

`insert into 테이블 values (...);` 형태에서 `insert`, `into`, 테이블 이름, `values`, 값 목록, `;`, 입력 끝을 순서대로 확인한다. 첫 번째 값은 B+Tree key로 사용할 `id`로 해석한다.

예시:

```sql
insert into users values (101, kim);
```

## 4. 실행

파싱된 계획을 기준으로 `SELECT` 또는 `INSERT` 동작을 실행하는 영역이다. 이 계층에서 전체 조회 경로, id 조건 조회 경로, row 추가 경로가 분기된다.

### 4.1 실행 분기

파싱 결과의 타입과 조건을 보고 실행 함수를 선택한다.

```text
SELECT without WHERE id -> fixed row 전체 스캔
SELECT with WHERE id    -> B+Tree index 조회
INSERT                  -> fixed row append + index put
```

### 4.2 SELECT 전체 조회

테이블 CSV 파일을 `0, 64, 128...` offset 순서로 읽고, 각 fixed row를 논리 row로 decode해 출력한다. 이 경로는 인덱스를 사용하지 않는다.

### 4.3 SELECT id 조건 조회

`WHERE id = ?` 조건의 id 값을 B+Tree key로 사용한다. `db_index_get()`으로 row offset을 찾고, 해당 위치로 `fseek()`한 뒤 정확히 64 bytes만 읽어 결과를 출력한다.

인덱스에서 id를 찾지 못하면 에러가 아니라 빈 결과로 처리한다.

### 4.4 INSERT 행 추가

INSERT 값 목록에서 첫 번째 컬럼인 `id`를 추출한다. 먼저 B+Tree로 중복 id 여부를 확인하고, 중복이 없으면 논리 row를 64-byte fixed row로 encoding한 뒤 파일 끝에 append한다.

파일 쓰기가 성공하면 새 row의 byte offset을 `db_index_put()`으로 등록한다.

## 5. 테이블 메타데이터 / 고정 길이 파일 저장

테이블 이름, 컬럼 목록, CSV 파일 경로, fixed row encoding 규칙을 연결하고 파일을 읽거나 쓰는 영역이다. 기존 CSV 줄 단위 저장은 64-byte fixed row 저장으로 바뀐다.

### 5.1 테이블 이름 매핑

테이블 이름을 컬럼 목록과 CSV 파일 경로가 들어 있는 메타데이터로 변환한다.

예시:

```text
users -> data/users.csv
```

### 5.2 64-byte fixed row encoding

논리 값들을 CSV 형태로 만든 뒤 padding 필드를 추가해 줄바꿈 전까지 63 bytes를 채우고, 마지막 1 byte는 `\n`으로 둔다.

예시:

```text
101,kim,_______________________________________________________\n
```

사용자에게는 padding 컬럼을 보여주지 않는다. padding은 파일 안에서 fixed row 크기를 맞추기 위한 내부 필드로만 사용한다.

### 5.3 fixed row decoding

파일에서 64 bytes를 읽은 뒤 padding 필드를 제거하고 논리 컬럼만 SELECT 결과로 출력한다.

### 5.4 offset 기반 읽기/쓰기

전체 조회는 파일 크기를 기준으로 64-byte 간격을 순차 스캔한다. id 조건 조회는 B+Tree가 반환한 offset으로 바로 이동해 단일 row만 읽는다. INSERT는 append 전 `ftell()`로 새 row의 시작 offset을 얻는다.

### 5.5 row 크기 검증

논리 row와 padding을 포함한 결과가 64 bytes에 들어가지 않으면 INSERT를 거부한다.

예시 출력:

```text
row size를 초과했습니다
```

## 6. B+Tree 인덱스 wrapper

외부 B+Tree 라이브러리와 미니 DB 실행기를 분리하는 영역이다. 라이브러리 타입과 세부 API는 이 계층 안에 숨긴다.

### 6.1 테이블별 인덱스 관리

`users`, `posts` 같은 테이블마다 별도의 B+Tree 인스턴스 또는 인덱스 파일을 관리한다.

### 6.2 공개 API 제한

실행기는 다음과 같은 작은 API만 사용한다.

```c
int db_index_init_all(void);
void db_index_shutdown_all(void);
int db_index_rebuild_table(const TableMetadata *table);
int db_index_put(const char *table_name, int id, long row_offset);
int db_index_get(const char *table_name, int id, long *row_offset);
```

### 6.3 offset 저장 규칙

`begeekmyfriend/bplustree`는 `data == 0`을 delete로 처리한다. 실제 파일 첫 row의 offset은 `0`일 수 있으므로 wrapper는 B+Tree에 실제 offset 대신 `offset + 1`을 저장한다.

읽을 때는 `stored_offset - 1`로 실제 offset을 복원한다.

### 6.4 인덱스 재구축

첫 구현에서는 CSV 파일을 기준 데이터로 보고, 프로그램 시작 시 CSV에서 인덱스를 다시 만든다. 이렇게 하면 CSV 파일과 인덱스 파일의 sync 문제가 생겼을 때도 기준 데이터를 명확하게 유지할 수 있다.

## 7. vendored B+Tree 라이브러리

실제 B+Tree 삽입, 검색, split, disk I/O를 담당하는 외부 구현 영역이다. 미니 DB의 다른 계층은 이 구현에 직접 의존하지 않는다.

### 7.1 vendoring 위치

`begeekmyfriend/bplustree`의 `disk-io` 브랜치를 기준으로 최소 파일을 `third_party/` 아래에 포함한다.

예시:

```text
third_party/bplustree/bplustree.c
third_party/bplustree/bplustree.h
third_party/bplustree/LICENSE
```

### 7.2 POSIX disk I/O

라이브러리는 `pread`, `pwrite`, `open`, `fsync` 같은 POSIX API를 사용한다. macOS와 Linux 개발 환경에서는 사용할 수 있지만, Windows 빌드는 별도 대응이 필요하다.

### 7.3 빌드 연결

`CMakeLists.txt`는 vendored B+Tree 소스와 `db_index.c` wrapper를 빌드 대상에 포함한다.

## 전체 계층 구조

```text
SQL 처리기
├── 1. 프로그램 시작 / 인덱스 준비 [신규]
│   ├── 1.1 테이블 메타데이터 확인 [신규]
│   ├── 1.2 B+Tree wrapper 초기화 [신규]
│   └── 1.3 CSV 기반 인덱스 재구축 [신규]
├── 2. REPL SQL 입력 처리
│   ├── 2.1 프롬프트 출력
│   ├── 2.2 SQL 한 줄 읽기
│   └── 2.3 특수 명령 처리 [변경: 종료 시 인덱스 자원 정리]
├── 3. SQL 파싱
│   ├── 3.1 SQL 타입 판별
│   ├── 3.2 SELECT 전체 조회 파싱
│   ├── 3.3 SELECT id 조건 조회 파싱 [신규]
│   └── 3.4 INSERT 테이블명과 값 목록 추출 [변경: 첫 번째 값을 id key로 사용]
├── 4. 실행
│   ├── 4.1 실행 분기
│   ├── 4.2 SELECT 전체 조회 [변경: 64-byte 단위 스캔]
│   ├── 4.3 SELECT id 조건 조회 [신규]
│   └── 4.4 INSERT 행 추가 [변경: fixed row append + index put]
├── 5. 테이블 메타데이터 / 고정 길이 파일 저장 [변경]
│   ├── 5.1 테이블 이름 매핑
│   ├── 5.2 64-byte fixed row encoding [신규]
│   ├── 5.3 fixed row decoding [신규]
│   ├── 5.4 offset 기반 읽기/쓰기 [신규]
│   └── 5.5 row 크기 검증 [신규]
├── 6. B+Tree 인덱스 wrapper [신규]
│   ├── 6.1 테이블별 인덱스 관리 [신규]
│   ├── 6.2 공개 API 제한 [신규]
│   ├── 6.3 offset 저장 규칙 [신규]
│   └── 6.4 인덱스 재구축 [신규]
└── 7. vendored B+Tree 라이브러리 [신규]
    ├── 7.1 vendoring 위치 [신규]
    ├── 7.2 POSIX disk I/O [신규]
    └── 7.3 빌드 연결 [신규]
```

## 시작 흐름

```text
프로그램 시작
-> 테이블 메타데이터 확인
-> B+Tree wrapper 초기화
-> CSV 파일을 64-byte row 단위로 스캔
-> 각 row의 id와 offset을 B+Tree에 등록
-> REPL 프롬프트 출력
```

## SELECT 전체 조회 흐름

```text
REPL 프롬프트 출력
-> SQL 한 줄 읽기
-> 특수 명령 처리
-> SELECT 전체 조회 파싱
-> 실행 분기
-> CSV 파일을 64-byte 단위로 순차 스캔
-> fixed row decoding
-> padding 제외 후 논리 컬럼과 row 출력
```

## SELECT id 조건 조회 흐름

```text
REPL 프롬프트 출력
-> SQL 한 줄 읽기
-> 특수 명령 처리
-> SELECT id 조건 조회 파싱
-> 실행 분기
-> db_index_get(table, id, &offset)
-> B+Tree wrapper가 stored_offset - 1로 실제 offset 복원
-> fseek(table_path, offset, SEEK_SET)
-> 64 bytes 읽기
-> fixed row decoding
-> padding 제외 후 논리 컬럼과 row 출력
```

## INSERT 흐름

```text
REPL 프롬프트 출력
-> SQL 한 줄 읽기
-> 특수 명령 처리
-> INSERT 테이블명과 값 목록 추출
-> 첫 번째 값을 id로 해석
-> db_index_get()으로 중복 id 확인
-> logical row를 64-byte fixed row로 encoding
-> ftell()로 append offset 확인
-> CSV 파일 끝에 fixed row 쓰기
-> db_index_put(table, id, offset)
-> B+Tree wrapper가 offset + 1 저장
```

## 계층 간 의존 방향

```text
REPL
-> Parser
-> Executor
-> Table Storage
-> db_index wrapper
-> vendored B+Tree library
```

파서와 실행기는 B+Tree 라이브러리의 내부 타입을 알지 않는다. 실행기는 `WHERE id = ?` 조건일 때만 `db_index` wrapper를 호출하고, 전체 조회는 고정 길이 파일 저장 계층만 사용한다.

## 기존 구조 대비 핵심 변화

- REPL 입력 처리 계층은 거의 그대로 유지된다.
- SQL 파싱 계층에 `WHERE id = ?` 조건 파싱이 추가된다.
- 실행 계층은 전체 조회와 id 조건 조회를 분리한다.
- CSV 저장 계층은 줄 단위 읽기/쓰기에서 64-byte fixed row 읽기/쓰기로 바뀐다.
- `id -> byte offset` 매핑을 담당하는 B+Tree 인덱스 wrapper 계층이 추가된다.
- 외부 B+Tree 구현은 `third_party/`에 vendoring하고, 직접 의존은 wrapper 안으로 제한한다.

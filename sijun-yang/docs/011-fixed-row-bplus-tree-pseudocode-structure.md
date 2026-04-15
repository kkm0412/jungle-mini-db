# 고정 길이 Row + B+Tree 추가 의사코드 구조

## 전제

이 문서는 `docs/003-pseudocode-structure.md`의 기본 REPL, 파서, 실행기 의사코드가 이미 있다고 가정한다. 따라서 기존 함수의 기본 동작은 다시 설명하지 않고, `docs/010-fixed-row-bplus-tree-components-hierarchy.md`에서 새로 추가되거나 변경되는 흐름만 다룬다.

추가되는 핵심은 다음과 같다.

- 프로그램 시작 시 테이블별 인덱스 파일을 준비한다.
- 인덱스 파일이 없거나 손상된 경우에만 데이터 파일을 기준으로 복구한다.
- `select * from 테이블 where id = 값;` 형태를 추가로 지원한다.
- 전체 조회는 고정 길이 row 단위로 순차 읽기한다.
- `id` 조건 조회는 B+Tree 인덱스로 row 위치를 찾은 뒤 해당 row만 읽는다.
- INSERT는 고정 길이 row를 데이터 파일 끝에 추가한 뒤 인덱스도 함께 갱신한다.

추가 지원 SQL:

```sql
select * from users where id = 1;
select * from posts where id = 1;
```

## 추가 테이블 / 저장소 정보

기존 테이블 정보에 고정 row 크기와 인덱스 파일 정보가 추가된다.

```text
GLOBAL_ROW_SIZE = fixed row size

GLOBAL_INDEX_FILES = {
    "users": users index file,
    "posts": posts index file
}
```

3단계:

1. 테이블별 데이터 파일은 기존처럼 유지한다.
2. 각 테이블은 데이터 파일과 짝이 되는 인덱스 파일을 가진다.
3. 모든 row는 같은 크기로 저장되며, 남는 공간은 내부 padding으로 채운다.

## main 시작 / 종료 변경

기존 REPL 루프 앞뒤에 인덱스 준비와 자원 정리 단계가 추가된다.

```text
def main()
    result = prepare_database()
    if result has error
        print result.error
        return 1

    while true
        print prompt
        sql = read and trim user input

        if sql == ".exit"
            shutdown_database()
            return 0

        plan = parse_sql(sql)
        if plan has error
            print plan.error
            continue

        execute_plan(plan)
```

3단계:

1. REPL을 시작하기 전에 데이터 파일과 인덱스 파일을 사용할 수 있는 상태로 준비한다.
2. 기존 REPL 입력, 파싱, 실행 흐름은 유지한다.
3. 종료 시 열린 자원을 정리한다.

## 프로그램 시작 준비

테이블별 인덱스 파일을 열고, 정상적으로 사용할 수 있는지 확인한다.

```text
def prepare_database()
    for each table in GLOBAL_TABLES
        result = open_index_file(table)
        if result has error
            return result

        if index file is missing or damaged
            result = rebuild_index_from_data_file(table)
            if result has error
                return result

    return success
```

3단계:

1. 모든 테이블에 대해 인덱스 파일을 연다.
2. 인덱스 파일 상태를 확인한다.
3. 인덱스 파일이 없거나 손상된 경우에만 데이터 파일을 기준으로 복구한다.

## 인덱스 복구

데이터 파일을 고정 길이 row 단위로 읽으면서 `id`와 row 위치를 인덱스에 등록한다.

```text
def rebuild_index_from_data_file(table_name)
    clear index for table_name

    position = first row position
    while position is inside data file
        raw_row = read fixed row at position
        if raw_row is invalid
            return_err "데이터 파일이 올바르지 않습니다"

        logical_row = decode_fixed_row(raw_row)
        id = first value of logical_row

        add id and position to index
        position = next row position

    return success
```

3단계:

1. 기존 인덱스 내용을 비운다.
2. 데이터 파일을 고정 길이 row 단위로 처음부터 끝까지 읽는다.
3. 각 row의 첫 번째 값인 `id`와 row 위치를 인덱스에 등록한다.

## SELECT 파서 변경

기존 전체 조회 파싱은 유지하고, `where id = 값` 조건이 붙은 형태만 추가로 인식한다.

입력 예시:

```sql
select * from users where id = 1;
```

인식 순서:

```text
is "select"
trim
is "*"
trim
is "from"
trim
get table_name
trim
optional "where id = value"
trim
is ";"
trim
is_end
```

파서 의사코드:

```text
def parse_select(sql)
    parse existing select prefix
    table_name = parsed table name

    if next token is ";"
        return {
            type: SELECT,
            table_name: table_name,
            condition: none
        }

    if next token is "where"
        parse "id"
        parse "="
        id_value = parse integer value
        parse ";"
        parse end

        return {
            type: SELECT,
            table_name: table_name,
            condition: {
                column: "id",
                value: id_value
            }
        }

    return_err "지원하지 않는 SELECT 문법입니다"
```

3단계:

1. 기존 `select * from 테이블` 부분은 기존 방식대로 파싱한다.
2. 바로 끝나면 전체 조회 계획을 만든다.
3. `where id = 값`이 있으면 `id` 조건 조회 계획을 만든다.

## SELECT 실행 변경

SELECT 실행은 조건 유무에 따라 전체 조회와 인덱스 조회로 나뉜다.

```text
def execute_select(plan)
    if plan has no condition
        execute_select_all_fixed_rows(plan)
        return

    if plan condition is id equality
        execute_select_by_id(plan)
        return

    print "지원하지 않는 조회 조건입니다"
```

3단계:

1. 조건이 없으면 전체 row를 순차적으로 읽는다.
2. `id` 조건이면 인덱스를 사용해 하나의 row 위치를 찾는다.
3. 그 외 조건은 지원하지 않는 조건으로 처리한다.

## 전체 조회 실행

데이터 파일을 고정 길이 row 단위로 순차 읽고, 내부 padding을 제거한 논리 row만 출력한다.

```text
def execute_select_all_fixed_rows(plan)
    print columns for plan.table_name

    position = first row position
    while position is inside data file
        raw_row = read fixed row at position
        logical_row = decode_fixed_row(raw_row)
        print logical_row
        position = next row position
```

3단계:

1. 테이블의 컬럼명을 출력한다.
2. 데이터 파일을 고정 길이 row 단위로 순차적으로 읽는다.
3. 내부 padding을 제거한 값만 출력한다.

## id 조건 조회 실행

인덱스에서 `id`에 해당하는 row 위치를 찾고, 그 위치의 row만 읽는다.

```text
def execute_select_by_id(plan)
    print columns for plan.table_name

    id = plan.condition.value
    result = find row position from index using id

    if result not found
        return

    raw_row = read fixed row at result.position
    logical_row = decode_fixed_row(raw_row)
    print logical_row
```

3단계:

1. `id` 값을 인덱스에서 검색한다.
2. 찾지 못하면 컬럼명만 출력하고 종료한다.
3. 찾으면 해당 위치의 row만 읽어 출력한다.

## INSERT 실행 변경

INSERT는 기존처럼 값을 파싱하되, 저장 방식과 인덱스 갱신이 바뀐다.

```text
def execute_insert(plan)
    id = first value of plan.values

    if index already contains id
        print "이미 존재하는 id입니다"
        return

    fixed_row = encode_fixed_row(plan.values)
    if fixed_row has error
        print fixed_row.error
        return

    position = append fixed_row to data file
    if append has error
        print "데이터를 저장할 수 없습니다"
        return

    result = add id and position to index
    if result has error
        print "인덱스를 갱신할 수 없습니다"
        return
```

3단계:

1. 첫 번째 값을 `id`로 보고 중복 여부를 인덱스에서 확인한다.
2. 값 목록을 고정 길이 row로 변환한 뒤 데이터 파일 끝에 추가한다.
3. 새 row의 위치를 인덱스에 등록한다.

## 고정 길이 row encoding

논리 값 목록을 파일에 저장 가능한 고정 길이 row로 바꾼다.

```text
def encode_fixed_row(values)
    logical_text = join values as one row

    if logical_text is too long for fixed row
        return_err "row size를 초과했습니다"

    padding = enough internal padding to fill fixed row
    fixed_row = logical_text + padding + row ending

    return fixed_row
```

3단계:

1. 사용자 입력 값을 하나의 논리 row 문자열로 만든다.
2. 고정 row 크기에 들어가는지 확인한다.
3. 남는 공간을 내부 padding으로 채워 고정 길이 row를 만든다.

## 고정 길이 row decoding

파일에서 읽은 고정 길이 row를 사용자에게 보여줄 논리 row로 바꾼다.

```text
def decode_fixed_row(raw_row)
    remove row ending from raw_row
    remove internal padding from raw_row
    values = split remaining text into values

    return values
```

3단계:

1. row 끝 표시를 제거한다.
2. 내부 padding을 제거한다.
3. 남은 값을 컬럼 순서에 맞는 논리 값 목록으로 바꾼다.

## 인덱스 동작

B+Tree 인덱스는 `id`와 row 위치를 연결하는 역할만 한다.

```text
def find row position from index using id
    search id in table index
    if id exists
        return row position
    return not found

def add id and position to index
    insert id and row position into table index
    return success or error
```

3단계:

1. 조회 시 `id`를 key로 사용해 row 위치를 찾는다.
2. 삽입 시 새 `id`와 row 위치를 인덱스에 추가한다.
3. B+Tree 내부의 정렬과 균형 유지 방식은 이 계층 밖에서 직접 다루지 않는다.

## 전체 실행 흐름

```text
main
├── prepare_database
│   ├── open index file for each table
│   └── rebuild damaged or missing index
└── REPL loop
    ├── read sql
    ├── .exit이면 shutdown_database 후 종료
    ├── parse_sql
    │   ├── parse_select
    │   │   ├── SELECT 전체 조회 계획
    │   │   └── SELECT id 조건 조회 계획
    │   └── parse_insert
    └── execute_plan
        ├── execute_select
        │   ├── execute_select_all_fixed_rows
        │   └── execute_select_by_id
        └── execute_insert
            ├── encode_fixed_row
            ├── append fixed row to data file
            └── add id and position to index
```

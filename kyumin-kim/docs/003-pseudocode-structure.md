# SQL 처리기 REPL 기반 최소 의사코드 구조

## 전제

처음 구현은 최대한 단순하게 한다.

- SQL 파일 입력보다 먼저 REPL 입력을 기준으로 동작을 잡는다.
- 복잡한 SQL 파서는 만들지 않는다.
- 공백, 대소문자, 문법 오류는 깊게 처리하지 않는다.
- 해피케이스 SQL만 처리한다.
- 테이블은 `users`, `posts`만 존재한다.
- 테이블별 컬럼은 코드 안에 전역 변수로 하드코딩되어 있다.
- CSV 파일은 프로젝트 루트의 `data/` 아래에 이미 존재한다고 가정한다.
- 모든 값의 타입은 ASCII 범위의 텍스트라고 가정한다.

최소 지원 SQL:

```sql
select * from users;
select * from posts;
insert into users values (1, kim);
insert into posts values (1, hello);
```

## 하드코딩된 테이블 정보

외부에서 스키마 정보를 가져오는 함수처럼 보이지만, 실제로는 전역 변수에 박아 둔 값을 조회한다.

```text
GLOBAL_TABLES = ["users", "posts"]

GLOBAL_COLUMNS = {
    "users": ["id", "name"],
    "posts": ["id", "title"]
}

PROJECT_ROOT_DIR = build-time project root path

GLOBAL_CSV_FILES = {
    "users": PROJECT_ROOT_DIR + "/data/users.csv",
    "posts": PROJECT_ROOT_DIR + "/data/posts.csv"
}
```

3단계:

1. 허용 테이블 목록을 전역 변수로 고정한다.
2. 테이블별 컬럼 목록도 전역 변수로 고정한다.
3. 테이블 이름으로 프로젝트 루트 기준 CSV 파일 경로를 전역 변수에서 가져온다.

## main REPL

`main()`은 프로그램 전체를 감싸는 반복 루프 역할만 한다.

```text
def main()
    while true
        print "mini-db> "
        sql = read user input
        sql = trim sql

        if sql == ".exit"
            return 0

        plan = parse_sql(sql)
        if plan has error
            print plan.error
            continue

        execute_plan(plan)
```

3단계:

1. 사용자에게 SQL을 입력받고 앞뒤 공백과 개행을 제거한다.
2. 특수 명령은 마침표(`.`)로 시작하며, 현재 지원되는 `.exit`이면 프로그램을 종료한다.
3. 그 외 입력은 `parse_sql(sql)`로 파싱한 뒤 `execute_plan(plan)`으로 실행한다.

## SQL 파서

`parse_sql()`은 SQL 문자열을 실행 가능한 계획으로 바꾼다.
SELECT와 INSERT의 세부 파서는 의사코드처럼 앞에서부터 읽고, 실패 처리는 파서 헬퍼 내부에서 처리한다.

```text
def parse_sql(sql)
    sql = trim sql

    if sql does not end with ";"
        return_err "SQL은 ;로 끝나야 합니다"

    if sql starts with "select"
        return parse_select(sql)

    if sql starts with "insert"
        return parse_insert(sql)

    return_err "지원하지 않는 SQL"
```

3단계:

1. SQL 앞뒤 공백을 정리한 뒤 마지막 의미 있는 문자가 `;`인지 확인한다.
2. SQL이 `select`로 시작하면 SELECT 파서로, `insert`로 시작하면 INSERT 파서로 보낸다.
3. 둘 다 아니면 파싱 오류를 반환한다.

## SQL 실행기

`execute_plan()`은 파싱된 계획만 보고 실제 파일 작업을 한다.

```text
def execute_plan(plan)
    if plan.type == SELECT
        execute_select(plan)
        return

    if plan.type == INSERT
        execute_insert(plan)
        return

    print "실행할 수 없는 계획입니다"
```

3단계:

1. `plan.type`이 `SELECT`인지 확인한다.
2. `plan.type`이 `INSERT`인지 확인한다.
3. 둘 다 아니면 실행할 수 없는 계획으로 처리한다.

## SELECT 최소 처리

처음에는 `select * from 테이블이름;` 형태만 처리한다.

입력 예시:

```sql
select * from users;
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
is ";"
trim
is_end
```

파서 의사코드:

```text
def parse_select(sql)
    cursor = sql

    is "select"
    trim
    is "*"
    trim
    is "from"
    trim
    table_name = get name
    trim
    is ";"
    trim
    is_end

    if table_name not in GLOBAL_TABLES
        return_err "존재하지 않는 테이블입니다"

    return {
        type: SELECT,
        table_name: table_name
    }
```

실행기 의사코드:

```text
def execute_select(plan)
    columns = GLOBAL_COLUMNS[plan.table_name]
    csv_file_path = GLOBAL_CSV_FILES[plan.table_name]

    file = open csv_file_path in read mode
    if file cannot be opened
        print "CSV 파일을 열 수 없습니다"
        return

    print columns
    print all rows from file
    close file
```

3단계:

1. 파서는 `select`, `*`, `from`, 테이블 이름, `;`, 입력 끝을 앞에서부터 순서대로 확인한다.
2. 파서는 테이블 이름이 `GLOBAL_TABLES`에 있는지 확인하고 `SELECT` 계획을 만든다.
3. 실행기는 계획의 테이블 이름으로 프로젝트 루트 기준 CSV 경로를 가져와 컬럼명과 전체 행을 출력한다.

## INSERT 최소 처리

처음에는 `insert into 테이블이름 values (...);` 형태만 처리한다.

입력 예시:

```sql
insert into users values (1, kim);
```

인식 순서:

```text
is "insert"
trim
is "into"
trim
get table_name
trim
is "values"
trim
get values
trim
is ";"
trim
is_end
```

파서 의사코드:

```text
def parse_insert(sql)
    cursor = sql

    is "insert"
    trim
    is "into"
    trim
    table_name = get name
    trim
    is "values"
    trim
    values = get text until ";"
    trim
    is ";"
    trim
    is_end

    values = remove "(" from values
    values = remove ")" from values
    values = trim values
    value_list = split values by ","
    value_list = trim each value

    if table_name not in GLOBAL_TABLES
        return_err "존재하지 않는 테이블입니다"

    columns = GLOBAL_COLUMNS[table_name]
    if count(value_list) != count(columns)
        return_err "컬럼 개수와 값 개수가 맞지 않습니다"

    if value_list contains non-ASCII text
        return_err "ASCII 텍스트만 입력할 수 있습니다"

    return {
        type: INSERT,
        table_name: table_name,
        values: value_list
    }
```

실행기 의사코드:

```text
def execute_insert(plan)
    csv_file_path = GLOBAL_CSV_FILES[plan.table_name]

    file = open csv_file_path in append/read mode
    if file cannot be opened
        print "CSV 파일을 열 수 없습니다"
        return

    if file is not empty and last character is not newline
        write newline to file

    write plan.values as one CSV row to file
    close file
```

3단계:

1. 파서는 `insert into` 뒤에서 테이블 이름과 `values (...)` 부분을 단순 문자열 기준으로 나눈다.
2. 파서는 테이블 존재 여부, 컬럼 개수와 값 개수 일치 여부, ASCII 텍스트 여부만 확인하고 `INSERT` 계획을 만든다.
3. 실행기는 계획의 값 목록을 이미 존재하는 CSV 파일 끝에 한 줄로 추가한다.

## 파일 저장소 최소 처리

파일 저장소는 별도 복잡한 계층 없이 전역 변수의 CSV 경로만 사용한다.

```text
table_name = "users"
csv_file_path = GLOBAL_CSV_FILES[table_name]
```

3단계:

1. 테이블 이름으로 `GLOBAL_CSV_FILES`에서 프로젝트 루트 기준 CSV 경로를 가져온다.
2. INSERT는 이미 존재하는 CSV 파일 끝에 한 줄을 추가한다.
3. SELECT는 이미 존재하는 CSV 파일 전체를 읽는다.

## 전체 실행 흐름

```text
main
└── REPL loop
    ├── read sql
    ├── .exit이면 종료
    ├── parse_sql
    │   ├── parse_select
    │   └── parse_insert
    └── execute_plan
        ├── execute_select
        └── execute_insert
```

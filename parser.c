#include <stdio.h>
#include <string.h>

#include "mini_db.h"

/* 파일 안에서만 사용: 아래 핵심 함수들이 호출하는 내부 함수 목록이다. */
static Plan parse_select(const char *sql);
static Plan parse_insert(const char *sql);
static int starts_with(const char *text, const char *prefix);
static void set_error(Plan *plan, const char *message);
static int is_ascii_text(const char *text);
static void remove_trailing_semicolon(char *text);
static char *trim(char *text);

/* 2.1 SQL 타입 판별: SELECT와 INSERT 중 어떤 문장인지 구분한다. */
Plan parse_sql(const char *sql) {
    Plan plan = {0};

    if (starts_with(sql, "select * from")) {
        /* 흐름: 2.1 SQL 타입 판별 -> 2.2 SELECT 테이블명 추출 */
        return parse_select(sql);
    }

    if (starts_with(sql, "insert into")) {
        /* 흐름: 2.1 SQL 타입 판별 -> 2.3 INSERT 테이블명과 값 목록 추출 */
        return parse_insert(sql);
    }

    set_error(&plan, "지원하지 않는 SQL");
    return plan;
}

/* 2.2 SELECT 테이블명 추출: `select * from 테이블;`에서 테이블 이름을 뽑아낸다. */
static Plan parse_select(const char *sql) {
    const char *prefix = "select * from";
    Plan plan = {0};
    char table_name[MAX_TABLE_NAME_SIZE];
    char *trimmed_table_name;

    snprintf(table_name, sizeof(table_name), "%s", sql + strlen(prefix));
    trimmed_table_name = trim(table_name);
    remove_trailing_semicolon(trimmed_table_name);
    trimmed_table_name = trim(trimmed_table_name);

    /* 흐름: 2.2 SELECT 테이블명 추출 -> 4.1 테이블 이름 매핑 */
    if (find_table(trimmed_table_name) == NULL) {
        set_error(&plan, "존재하지 않는 테이블입니다");
        return plan;
    }

    plan.type = QUERY_SELECT;
    snprintf(plan.table_name, sizeof(plan.table_name), "%s", trimmed_table_name);
    return plan;
}

/* 2.3 INSERT 테이블명과 값 목록 추출: `insert into 테이블 values (...);`에서 테이블과 값 목록을 뽑아낸다. */
static Plan parse_insert(const char *sql) {
    const char *prefix = "insert into";
    Plan plan = {0};
    char rest[MAX_INPUT_SIZE];
    char values_text[MAX_INPUT_SIZE];
    char *values_keyword;
    char *table_name;
    char *values;
    char *token;
    const TableMetadata *table;

    snprintf(rest, sizeof(rest), "%s", sql + strlen(prefix));
    values_keyword = strstr(rest, "values");

    if (values_keyword == NULL) {
        set_error(&plan, "지원하지 않는 SQL");
        return plan;
    }

    *values_keyword = '\0';
    table_name = trim(rest);
    values = trim(values_keyword + strlen("values"));

    remove_trailing_semicolon(values);
    values = trim(values);

    if (*values == '(') {
        values++;
    }

    values = trim(values);
    if (strlen(values) > 0 && values[strlen(values) - 1] == ')') {
        values[strlen(values) - 1] = '\0';
    }

    values = trim(values);

    /* 흐름: 2.3 INSERT 테이블명과 값 목록 추출 -> 4.1 테이블 이름 매핑 */
    table = find_table(table_name);
    if (table == NULL) {
        set_error(&plan, "존재하지 않는 테이블입니다");
        return plan;
    }

    snprintf(values_text, sizeof(values_text), "%s", values);
    token = strtok(values_text, ",");

    while (token != NULL && plan.value_count < MAX_VALUES) {
        char *value = trim(token);

        if (!is_ascii_text(value)) {
            set_error(&plan, "ASCII 텍스트만 입력할 수 있습니다");
            return plan;
        }

        snprintf(plan.values[plan.value_count], sizeof(plan.values[plan.value_count]), "%s", value);
        plan.value_count++;
        token = strtok(NULL, ",");
    }

    if (token != NULL || plan.value_count != table->column_count) {
        set_error(&plan, "컬럼 개수와 값 개수가 맞지 않습니다");
        return plan;
    }

    plan.type = QUERY_INSERT;
    snprintf(plan.table_name, sizeof(plan.table_name), "%s", table_name);
    return plan;
}

/* 내부 처리: SQL이 지정한 키워드로 시작하는지 확인한다. */
static int starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

/* 내부 처리: Plan을 실패 상태로 바꾸고 사용자에게 보여줄 오류 메시지를 저장한다. */
static void set_error(Plan *plan, const char *message) {
    plan->type = QUERY_INVALID;
    snprintf(plan->error_message, sizeof(plan->error_message), "%s", message);
}

/* 내부 처리: INSERT 값에 ASCII 범위를 벗어나는 문자가 있는지 확인한다. */
static int is_ascii_text(const char *text) {
    const unsigned char *cursor = (const unsigned char *) text;

    while (*cursor != '\0') {
        if (*cursor > 127) {
            return 0;
        }
        cursor++;
    }

    return 1;
}

/* 내부 처리: SQL 끝의 세미콜론 하나를 제거한다. */
static void remove_trailing_semicolon(char *text) {
    size_t length = strlen(text);

    if (length > 0 && text[length - 1] == ';') {
        text[length - 1] = '\0';
    }
}

/* 내부 처리: 파싱 중 잘라낸 문자열 조각의 앞뒤 공백과 개행을 제거한다. */
static char *trim(char *text) {
    char *end;

    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return text;
}

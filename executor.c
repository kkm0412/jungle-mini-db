#include <stdio.h>
#include <string.h>

#include "mini_db.h"

/* 파일 안에서만 사용: 아래 핵심 함수들이 호출하는 내부 함수 목록이다. */
static void execute_select(const Plan *plan);
static void execute_insert(const Plan *plan);
static void print_columns(const TableMetadata *table);
static void write_newline_if_needed(FILE *file);
static void write_values(FILE *file, const Plan *plan);

/* 3.1 실행 분기: 파싱된 계획을 SELECT 또는 INSERT 실행으로 보낸다. */
void execute_plan(const Plan *plan) {
    if (plan->type == QUERY_SELECT) {
        /* 흐름: 3.1 실행 분기 -> 3.2 SELECT 전체 조회 */
        execute_select(plan);
        return;
    }

    if (plan->type == QUERY_INSERT) {
        /* 흐름: 3.1 실행 분기 -> 3.3 INSERT 행 추가 */
        execute_insert(plan);
        return;
    }

    printf("실행할 수 없는 계획입니다\n");
}

/* 3.2 SELECT 전체 조회: 테이블 CSV 파일을 읽어 컬럼명과 모든 행을 출력한다. */
static void execute_select(const Plan *plan) {
    /* 흐름: 3.2 SELECT 전체 조회 -> 4.1 테이블 이름 매핑 */
    const TableMetadata *table = find_table(plan->table_name);
    FILE *file;
    char row[MAX_INPUT_SIZE];

    if (table == NULL) {
        printf("실행할 수 없는 계획입니다\n");
        return;
    }

    /* 흐름: 4.1 테이블 이름 매핑 -> 4.2 CSV 파일 열기 */
    file = fopen(table->csv_file_path, "r");
    if (file == NULL) {
        printf("CSV 파일을 열 수 없습니다\n");
        return;
    }

    /* 흐름: 4.2 CSV 파일 열기 -> 4.3 CSV 읽기/쓰기 */
    print_columns(table);
    while (fgets(row, sizeof(row), file) != NULL) {
        printf("%s", row);
        if (strlen(row) > 0 && row[strlen(row) - 1] != '\n') {
            printf("\n");
        }
    }

    fclose(file);
}

/* 3.3 INSERT 행 추가: 파싱된 값 목록을 테이블 CSV 파일 끝에 추가한다. */
static void execute_insert(const Plan *plan) {
    /* 흐름: 3.3 INSERT 행 추가 -> 4.1 테이블 이름 매핑 */
    const TableMetadata *table = find_table(plan->table_name);
    FILE *file;

    if (table == NULL) {
        printf("실행할 수 없는 계획입니다\n");
        return;
    }

    /* 흐름: 4.1 테이블 이름 매핑 -> 4.2 CSV 파일 열기 */
    file = fopen(table->csv_file_path, "a+");
    if (file == NULL) {
        printf("CSV 파일을 열 수 없습니다\n");
        return;
    }

    /* 흐름: 4.2 CSV 파일 열기 -> 4.3 CSV 읽기/쓰기 */
    write_newline_if_needed(file);
    write_values(file, plan);
    fclose(file);
}

/* 내부 처리: 4.3 CSV 읽기/쓰기 전에 컬럼명을 CSV 형태로 출력한다. */
static void print_columns(const TableMetadata *table) {
    for (int i = 0; i < table->column_count; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s", table->columns[i]);
    }
    printf("\n");
}

/* 내부 처리: 4.3 CSV 읽기/쓰기 중 새 INSERT 행이 붙지 않도록 개행을 추가한다. */
static void write_newline_if_needed(FILE *file) {
    long file_size;
    int last_char;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    if (file_size <= 0) {
        return;
    }

    fseek(file, -1, SEEK_END);
    last_char = fgetc(file);
    if (last_char != '\n') {
        fprintf(file, "\n");
    }
}

/* 내부 처리: 4.3 CSV 읽기/쓰기 중 INSERT 계획의 값 목록을 CSV 한 줄로 쓴다. */
static void write_values(FILE *file, const Plan *plan) {
    for (int i = 0; i < plan->value_count; i++) {
        if (i > 0) {
            fprintf(file, ",");
        }
        fprintf(file, "%s", plan->values[i]);
    }
    fprintf(file, "\n");
}

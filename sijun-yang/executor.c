#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_db.h"

/* 내부 구현: 실행 흐름을 구성하는 private 함수 목록이다. */
static void execute_select(const Plan *plan);
static void execute_select_all_fixed_rows(const TableMetadata *table);
static void execute_select_by_id(const Plan *plan, const TableMetadata *table);
static void execute_insert(const Plan *plan);
static void print_columns(const TableMetadata *table);
static int encode_fixed_row(const Plan *plan, char fixed_row[FIXED_ROW_SIZE]);
static int decode_fixed_row(const char fixed_row[FIXED_ROW_SIZE], char *logical_row, size_t logical_row_size);
static int parse_id_value(const char *text, int *id);
static const char *append_fixed_row(const TableMetadata *table, const char fixed_row[FIXED_ROW_SIZE],RowLocation *location);
static int read_and_decode_fixed_row(const TableMetadata *table, RowLocation location, char *logical_row,size_t logical_row_size);
static int read_fixed_row_at(const TableMetadata *table, RowLocation location, char fixed_row[FIXED_ROW_SIZE]);

/* 4.1 실행 분기: 파싱된 계획을 SELECT 또는 INSERT 실행으로 보낸다. */
void execute_plan(const Plan *plan) {
    if (plan->type == QUERY_SELECT) {
        execute_select(plan);
        return;
    }

    if (plan->type == QUERY_INSERT) {
        execute_insert(plan);
        return;
    }

    printf("실행할 수 없는 계획입니다\n");
}

/* 4.1 실행 분기: SELECT 조건 유무에 따라 전체 조회와 id 인덱스 조회로 나눈다. */
static void execute_select(const Plan *plan) {
    const TableMetadata *table = find_table(plan->table_name);

    if (table == NULL) {
        printf("실행할 수 없는 계획입니다\n");
        return;
    }

    if (plan->condition.type == SELECT_CONDITION_NONE) {
        execute_select_all_fixed_rows(table);
        return;
    }

    if (plan->condition.type == SELECT_CONDITION_ID_EQUALS) {
        execute_select_by_id(plan, table);
        return;
    }

    printf("지원하지 않는 조회 조건입니다\n");
}

/* 4.2 SELECT 전체 조회: 데이터 파일을 고정 길이 row 단위로 읽어 출력한다. */
static void execute_select_all_fixed_rows(const TableMetadata *table) {
    FILE *file;
    char fixed_row[FIXED_ROW_SIZE];
    char logical_row[MAX_LOGICAL_ROW_SIZE];

    file = fopen(table->csv_file_path, "rb");
    if (file == NULL) {
        printf("CSV 파일을 열 수 없습니다\n");
        return;
    }

    print_columns(table);
    while (1) {
        size_t read_size = fread(fixed_row, 1, FIXED_ROW_SIZE, file);

        if (read_size == 0) {
            break;
        }

        if (read_size != FIXED_ROW_SIZE || !decode_fixed_row(fixed_row, logical_row, sizeof(logical_row))) {
            printf("데이터 파일이 올바르지 않습니다\n");
            break;
        }

        printf("%s\n", logical_row);
    }

    fclose(file);
}

/* 4.3 SELECT id 조건 조회: B+Tree 인덱스에서 위치를 찾고 해당 row만 읽는다. */
static void execute_select_by_id(const Plan *plan, const TableMetadata *table) {
    RowLocation location;
    char logical_row[MAX_LOGICAL_ROW_SIZE];
    int found;

    print_columns(table);
    found = db_index_get(plan->table_name, plan->condition.id_value, &location);
    if (found == 0) {
        return;
    }
    if (found < 0) {
        printf("인덱스를 조회할 수 없습니다\n");
        return;
    }

    if (!read_and_decode_fixed_row(table, location, logical_row, sizeof(logical_row))) {
        printf("데이터 파일이 올바르지 않습니다\n");
        return;
    }

    printf("%s\n", logical_row);
}

/* 4.4 INSERT 행 추가: 고정 길이 row를 파일 끝에 쓰고 인덱스를 갱신한다. */
static void execute_insert(const Plan *plan) {
    const TableMetadata *table = find_table(plan->table_name);
    RowLocation location;
    char fixed_row[FIXED_ROW_SIZE];
    const char *append_error;
    int id;
    int found;

    if (table == NULL) {
        printf("실행할 수 없는 계획입니다\n");
        return;
    }

    if (!parse_id_value(plan->values[0], &id)) {
        printf("id는 정수여야 합니다\n");
        return;
    }

    found = db_index_get(plan->table_name, id, &location);
    if (found > 0) {
        printf("이미 존재하는 id입니다\n");
        return;
    }
    if (found < 0) {
        printf("인덱스를 조회할 수 없습니다\n");
        return;
    }

    if (!encode_fixed_row(plan, fixed_row)) {
        printf("row size를 초과했습니다\n");
        return;
    }

    append_error = append_fixed_row(table, fixed_row, &location);
    if (append_error != NULL) {
        printf("%s\n", append_error);
        return;
    }

    if (db_index_put(plan->table_name, id, location) != 1) {
        printf("인덱스를 갱신할 수 없습니다\n");
    }
}

static const char *append_fixed_row(const TableMetadata *table, const char fixed_row[FIXED_ROW_SIZE],
                                    RowLocation *location) {
    FILE *file = fopen(table->csv_file_path, "ab+");

    if (file == NULL) {
        return "CSV 파일을 열 수 없습니다";
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return "데이터를 저장할 수 없습니다";
    }

    location->offset = ftell(file);
    if (location->offset < 0 || location->offset % table->row_size != 0 ||
        fwrite(fixed_row, 1, FIXED_ROW_SIZE, file) != FIXED_ROW_SIZE || fflush(file) != 0) {
        fclose(file);
        return "데이터를 저장할 수 없습니다";
    }

    fclose(file);
    return NULL;
}

/* 내부 구현: SELECT 출력 전에 컬럼명을 CSV 형태로 출력한다. */
static void print_columns(const TableMetadata *table) {
    for (int i = 0; i < table->column_count; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%s", table->columns[i]);
    }
    printf("\n");
}

/* 5.2 고정 길이 row 저장: INSERT 값 목록을 64 bytes fixed row로 변환한다. */
static int encode_fixed_row(const Plan *plan, char fixed_row[FIXED_ROW_SIZE]) {
    size_t length = 0;

    memset(fixed_row, FIXED_ROW_PADDING_CHAR, FIXED_ROW_SIZE);  // 기본 패딩 문자열로 다 설정
    for (int i = 0; i < plan->value_count; i++) {   // value만큼 반복하면서 fixed_row 버퍼에 row 값 저장하기
        size_t value_length = strlen(plan->values[i]);

        if (length + value_length + 1 > FIXED_ROW_DATA_SIZE) {
            return 0;
        }

        memcpy(fixed_row + length, plan->values[i], value_length);
        length += value_length;
        fixed_row[length] = ',';
        length++;
    }

    fixed_row[FIXED_ROW_DATA_SIZE] = '\n';
    return 1;
}

/* 5.3 논리 row 변환: fixed row의 padding을 제거하고 출력용 row로 바꾼다. */
static int decode_fixed_row(const char fixed_row[FIXED_ROW_SIZE], char *logical_row, size_t logical_row_size) {
    int end = FIXED_ROW_DATA_SIZE - 1;

    if (logical_row_size < FIXED_ROW_SIZE || fixed_row[FIXED_ROW_DATA_SIZE] != '\n') {
        return 0;
    }

    memcpy(logical_row, fixed_row, FIXED_ROW_DATA_SIZE);
    logical_row[FIXED_ROW_DATA_SIZE] = '\0';

    while (end >= 0 && logical_row[end] == FIXED_ROW_PADDING_CHAR) {
        logical_row[end] = '\0';
        end--;
    }

    if (end >= 0 && logical_row[end] == ',') {
        logical_row[end] = '\0';
    }

    return 1;
}

/* 내부 구현: B+Tree key로 사용할 첫 번째 컬럼 id를 정수로 해석한다. */
static int parse_id_value(const char *text, int *id) {
    char *end;
    long parsed;

    if (text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (*end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *id = (int) parsed;
    return 1;
}

static int read_and_decode_fixed_row(const TableMetadata *table, RowLocation location, char *logical_row,
                                     size_t logical_row_size) {
    char fixed_row[FIXED_ROW_SIZE];

    return read_fixed_row_at(table, location, fixed_row) &&
           decode_fixed_row(fixed_row, logical_row, logical_row_size);
}

/* 5.4 위치 기반 읽기/쓰기: 인덱스가 알려준 byte offset에서 fixed row 하나를 읽는다. */
static int read_fixed_row_at(const TableMetadata *table, RowLocation location, char fixed_row[FIXED_ROW_SIZE]) {
    FILE *file;
    size_t read_size;

    if (location.offset < 0 || location.offset % table->row_size != 0) {
        return 0;
    }

    file = fopen(table->csv_file_path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, location.offset, SEEK_SET) != 0) {  //https://en.cppreference.com/w/c/io/fseek
        fclose(file);
        return 0;
    }

    read_size = fread(fixed_row, 1, FIXED_ROW_SIZE, file);
    fclose(file);
    return read_size == FIXED_ROW_SIZE;
}

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_db.h"

/* 내부 구현: 실행 흐름을 구성하는 private 함수 목록이다. */
static void execute_select(const Plan *plan);
static void execute_select_all_fixed_rows(const TableMetadata *table);
static void execute_select_by_id(const Plan *plan, const TableMetadata *table);
static void execute_select_by_id_range(const Plan *plan, const TableMetadata *table);
static void execute_insert(const Plan *plan);
static void print_columns(const TableMetadata *table);
static int encode_fixed_row(const Plan *plan, char fixed_row[ROW_SIZE]);
static int decode_fixed_row(const char fixed_row[ROW_SIZE], char *logical_row, size_t logical_row_size);
static int parse_id_value(const char *text, int *id);
static int extract_logical_row_id(const char *logical_row, int *id);
static int read_fixed_row_at(const TableMetadata *table, RowLocation location, char fixed_row[ROW_SIZE]);

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

    if (plan->condition.type == SELECT_CONDITION_ID_RANGE) {
        execute_select_by_id_range(plan, table);
        return;
    }

    printf("지원하지 않는 조회 조건입니다\n");
}

/* 4.2 SELECT 전체 조회: 데이터 파일을 고정 길이 row 단위로 읽어 출력한다. */
static void execute_select_all_fixed_rows(const TableMetadata *table) {
    FILE *file;
    char fixed_row[ROW_SIZE];
    char logical_row[MAX_INPUT_SIZE];

    file = fopen(table->csv_file_path, "rb");
    if (file == NULL) {
        printf("CSV 파일을 열 수 없습니다\n");
        return;
    }

    print_columns(table);
    while (1) {
        size_t read_size = fread(fixed_row, 1, ROW_SIZE, file);

        if (read_size == 0) {
            break;
        }

        if (read_size != ROW_SIZE || !decode_fixed_row(fixed_row, logical_row, sizeof(logical_row))) {
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
    char fixed_row[ROW_SIZE];
    char logical_row[MAX_INPUT_SIZE];
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

    if (!read_fixed_row_at(table, location, fixed_row) ||
        !decode_fixed_row(fixed_row, logical_row, sizeof(logical_row))) {
        printf("데이터 파일이 올바르지 않습니다\n");
        return;
    }

    printf("%s\n", logical_row);
}

/* 4.3 SELECT id 범위 조회: B+Tree leaf 링크를 따라 row 위치를 모으고 범위 안의 row만 출력한다. */
static void execute_select_by_id_range(const Plan *plan, const TableMetadata *table) {
    RowLocation *locations;
    long range_size;
    int count;

    if (plan->condition.id_value > plan->condition.id_end_value) {
        printf("조회 범위가 올바르지 않습니다\n");
        return;
    }

    range_size = (long) plan->condition.id_end_value - plan->condition.id_value + 1;
    if (range_size <= 0 || range_size > INT_MAX) {
        printf("조회 범위가 올바르지 않습니다\n");
        return;
    }

    locations = malloc(sizeof(*locations) * (size_t) range_size);
    if (locations == NULL) {
        printf("조회 결과를 준비할 수 없습니다\n");
        return;
    }

    count = db_index_scan_leafs_from(plan->table_name, plan->condition.id_value, (int) range_size, locations,
                                     (int) range_size);
    if (count < 0) {
        printf("인덱스를 조회할 수 없습니다\n");
        free(locations);
        return;
    }

    print_columns(table);
    for (int i = 0; i < count; i++) {
        char fixed_row[ROW_SIZE];
        char logical_row[MAX_INPUT_SIZE];
        int id;

        if (!read_fixed_row_at(table, locations[i], fixed_row) ||
            !decode_fixed_row(fixed_row, logical_row, sizeof(logical_row)) ||
            !extract_logical_row_id(logical_row, &id)) {
            printf("데이터 파일이 올바르지 않습니다\n");
            free(locations);
            return;
        }

        if (id > plan->condition.id_end_value) {
            break;
        }
        if (id >= plan->condition.id_value) {
            printf("%s\n", logical_row);
        }
    }

    free(locations);
}

/* 4.4 INSERT 행 추가: 고정 길이 row를 파일 끝에 쓰고 인덱스를 갱신한다. */
static void execute_insert(const Plan *plan) {
    const TableMetadata *table = find_table(plan->table_name);
    FILE *file;
    RowLocation location;
    char fixed_row[ROW_SIZE];
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

    file = fopen(table->csv_file_path, "ab+");
    if (file == NULL) {
        printf("CSV 파일을 열 수 없습니다\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    location.offset = ftell(file);
    if (location.offset < 0 || location.offset % table->row_size != 0 ||
        fwrite(fixed_row, 1, ROW_SIZE, file) != ROW_SIZE || fflush(file) != 0) {
        printf("데이터를 저장할 수 없습니다\n");
        fclose(file);
        return;
    }

    fclose(file);

    if (db_index_put(plan->table_name, id, location) != 1) {
        printf("인덱스를 갱신할 수 없습니다\n");
    }
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
static int encode_fixed_row(const Plan *plan, char fixed_row[ROW_SIZE]) {
    size_t length = 0;

    memset(fixed_row, ROW_PADDING_CHAR, ROW_SIZE);
    for (int i = 0; i < plan->value_count; i++) {
        size_t value_length = strlen(plan->values[i]);

        if (length + value_length + 1 > ROW_DATA_SIZE) {
            return 0;
        }

        memcpy(fixed_row + length, plan->values[i], value_length);
        length += value_length;
        fixed_row[length] = ',';
        length++;
    }

    fixed_row[ROW_DATA_SIZE] = '\n';
    return 1;
}

/* 5.3 논리 row 변환: fixed row의 padding을 제거하고 출력용 row로 바꾼다. */
static int decode_fixed_row(const char fixed_row[ROW_SIZE], char *logical_row, size_t logical_row_size) {
    int end = ROW_DATA_SIZE - 1;

    if (logical_row_size < ROW_SIZE || fixed_row[ROW_DATA_SIZE] != '\n') {
        return 0;
    }

    memcpy(logical_row, fixed_row, ROW_DATA_SIZE);
    logical_row[ROW_DATA_SIZE] = '\0';

    while (end >= 0 && logical_row[end] == ROW_PADDING_CHAR) {
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

/* 내부 구현: CSV 형태의 논리 row에서 첫 번째 컬럼 id만 정수로 추출한다. */
static int extract_logical_row_id(const char *logical_row, int *id) {
    char id_text[MAX_VALUE_SIZE];
    const char *comma = strchr(logical_row, ',');
    size_t length;

    if (comma == NULL) {
        return 0;
    }

    length = (size_t) (comma - logical_row);
    if (length == 0 || length >= sizeof(id_text)) {
        return 0;
    }

    memcpy(id_text, logical_row, length);
    id_text[length] = '\0';
    return parse_id_value(id_text, id);
}

/* 5.4 위치 기반 읽기/쓰기: 인덱스가 알려준 byte offset에서 fixed row 하나를 읽는다. */
static int read_fixed_row_at(const TableMetadata *table, RowLocation location, char fixed_row[ROW_SIZE]) {
    FILE *file;
    size_t read_size;

    if (location.offset < 0 || location.offset % table->row_size != 0) {
        return 0;
    }

    file = fopen(table->csv_file_path, "rb");
    if (file == NULL) {
        return 0;
    }

    if (fseek(file, location.offset, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    read_size = fread(fixed_row, 1, ROW_SIZE, file);
    fclose(file);
    return read_size == ROW_SIZE;
}

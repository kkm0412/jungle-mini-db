#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mini_db.h"
#include "thirdparty/bplustree.h"

#define BPLUS_BLOCK_SIZE 4096

typedef struct {
    const TableMetadata *table;
    struct bplus_tree *tree;
} IndexHandle;

/* 내부 구현: 공개 인덱스 흐름을 구성하는 private 함수 목록이다. */
static int rebuild_index_from_data_file(IndexHandle *handle, char *error_message, size_t error_size);
static int validate_index_against_data(IndexHandle *handle, char *error_message, size_t error_size);
static int read_data_row(FILE *file, const TableMetadata *table, long offset, char fixed_row[ROW_SIZE]);
static int extract_id_from_fixed_row(const char fixed_row[ROW_SIZE], int *id);
static IndexHandle *find_index_handle(const char *table_name);
static IndexHandle *ensure_index_handle(const TableMetadata *table);
static int ensure_data_file(const TableMetadata *table, char *error_message, size_t error_size);
static int init_tree(IndexHandle *handle, char *error_message, size_t error_size);
static int index_files_exist(const TableMetadata *table);
static void remove_index_files(const TableMetadata *table);
static int parse_int_text(const char *text, int *value);
static long get_file_size(FILE *file);
static long encode_offset_for_index(long offset);
static long decode_offset_from_index(long stored_offset);
static void set_error(char *error_message, size_t error_size, const char *message);

static IndexHandle INDEX_HANDLES[MAX_VALUES];
static int INDEX_HANDLE_COUNT = 0;

/* 1.2/1.3 인덱스 파일 열기와 상태 확인: 테이블별 B+Tree를 준비하고 필요하면 복구한다. */
int db_index_open_table(const TableMetadata *table, char *error_message, size_t error_size) {
    IndexHandle *handle;
    int validate_result;

    if (table->row_size != ROW_SIZE) {
        set_error(error_message, error_size, "지원하지 않는 row size입니다");
        return -1;
    }

    if (!ensure_data_file(table, error_message, error_size)) {
        return -1;
    }

    handle = ensure_index_handle(table);
    if (handle == NULL) {
        set_error(error_message, error_size, "인덱스를 준비할 수 없습니다");
        return -1;
    }

    if (!index_files_exist(table)) {
        return rebuild_index_from_data_file(handle, error_message, error_size);
    }

    if (!init_tree(handle, error_message, error_size)) {
        return -1;
    }

    validate_result = validate_index_against_data(handle, error_message, error_size);
    if (validate_result < 0) {
        return -1;
    }
    if (validate_result > 0) {
        return 0;
    }

    bplus_tree_deinit(handle->tree);
    handle->tree = NULL;
    return rebuild_index_from_data_file(handle, error_message, error_size);
}

/* 2.3 특수 명령 처리: 종료 시 열린 B+Tree 인덱스 자원을 정리한다. */
void db_index_shutdown_all(void) {
    for (int i = 0; i < INDEX_HANDLE_COUNT; i++) {
        if (INDEX_HANDLES[i].tree != NULL) {
            bplus_tree_deinit(INDEX_HANDLES[i].tree);
            INDEX_HANDLES[i].tree = NULL;
        }
        INDEX_HANDLES[i].table = NULL;
    }

    INDEX_HANDLE_COUNT = 0;
}

/* 6.3 조회 경로 제공: id key로 B+Tree를 검색해 fixed row byte offset을 반환한다. */
int db_index_get(const char *table_name, int id, RowLocation *location) {
    IndexHandle *handle = find_index_handle(table_name);
    long stored_offset;

    if (handle == NULL || handle->tree == NULL) {
        return -1;
    }

    stored_offset = bplus_tree_get(handle->tree, id);
    if (stored_offset < 0) {
        return 0;
    }

    location->offset = decode_offset_from_index(stored_offset);
    if (location->offset < 0 || location->offset % handle->table->row_size != 0) {
        return -1;
    }

    return 1;
}

/* 6.4 삽입 후 갱신: 새 id와 row 위치를 B+Tree 인덱스에 등록한다. */
int db_index_put(const char *table_name, int id, RowLocation location) {
    IndexHandle *handle = find_index_handle(table_name);
    long stored_offset;

    if (handle == NULL || handle->tree == NULL || location.offset < 0 ||
        location.offset % handle->table->row_size != 0) {
        return -1;
    }

    stored_offset = encode_offset_for_index(location.offset);
    if (stored_offset <= 0) {
        return -1;
    }

    return bplus_tree_put(handle->tree, id, stored_offset) == 0 ? 1 : -1;
}

/* 6.5 leaf 스캔 경로 제공: start_id가 들어갈 leaf부터 여러 leaf의 row 위치를 모은다. */
int db_index_scan_leafs_from(const char *table_name, int start_id, int leaf_count, RowLocation *locations,
                             int max_locations) {
    IndexHandle *handle = find_index_handle(table_name);
    long *stored_offsets;
    int count;

    if (handle == NULL || handle->tree == NULL || leaf_count <= 0 || max_locations < 0 ||
        (max_locations > 0 && locations == NULL)) {
        return -1;
    }

    if (max_locations == 0) {
        return 0;
    }

    stored_offsets = malloc(sizeof(*stored_offsets) * (size_t) max_locations);
    if (stored_offsets == NULL) {
        return -1;
    }

    count = bplus_tree_scan_leafs_from(handle->tree, start_id, leaf_count, stored_offsets, max_locations);
    if (count < 0) {
        free(stored_offsets);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        locations[i].offset = decode_offset_from_index(stored_offsets[i]);
        if (locations[i].offset < 0 || locations[i].offset % handle->table->row_size != 0) {
            free(stored_offsets);
            return -1;
        }
    }

    free(stored_offsets);
    return count;
}

/* 1.3 인덱스 상태 확인 / 필요 시 복구: 데이터 파일 기준으로 B+Tree를 다시 만든다. */
static int rebuild_index_from_data_file(IndexHandle *handle, char *error_message, size_t error_size) {
    FILE *file;
    long file_size;

    if (handle->tree != NULL) {
        bplus_tree_deinit(handle->tree);
        handle->tree = NULL;
    }

    remove_index_files(handle->table);
    if (!init_tree(handle, error_message, error_size)) {
        return -1;
    }

    file = fopen(handle->table->csv_file_path, "rb");
    if (file == NULL) {
        set_error(error_message, error_size, "CSV 파일을 열 수 없습니다");
        return -1;
    }

    file_size = get_file_size(file);
    if (file_size < 0 || file_size % handle->table->row_size != 0) {
        fclose(file);
        set_error(error_message, error_size, "데이터 파일이 올바르지 않습니다");
        return -1;
    }

    for (long offset = 0; offset < file_size; offset += handle->table->row_size) {
        char fixed_row[ROW_SIZE];
        int id;
        long stored_offset = encode_offset_for_index(offset);

        if (!read_data_row(file, handle->table, offset, fixed_row) || !extract_id_from_fixed_row(fixed_row, &id) ||
            stored_offset <= 0 || bplus_tree_put(handle->tree, id, stored_offset) != 0) {
            fclose(file);
            set_error(error_message, error_size, "데이터 파일이 올바르지 않습니다");
            return -1;
        }
    }

    fclose(file);
    return 0;
}

/* 1.3 인덱스 상태 확인: 인덱스의 id -> offset 매핑이 데이터 파일과 맞는지 확인한다. */
static int validate_index_against_data(IndexHandle *handle, char *error_message, size_t error_size) {
    FILE *file = fopen(handle->table->csv_file_path, "rb");
    long file_size;

    if (file == NULL) {
        set_error(error_message, error_size, "CSV 파일을 열 수 없습니다");
        return -1;
    }

    file_size = get_file_size(file);
    if (file_size < 0 || file_size % handle->table->row_size != 0) {
        fclose(file);
        set_error(error_message, error_size, "데이터 파일이 올바르지 않습니다");
        return -1;
    }

    for (long offset = 0; offset < file_size; offset += handle->table->row_size) {
        char fixed_row[ROW_SIZE];
        int id;
        long stored_offset;

        if (!read_data_row(file, handle->table, offset, fixed_row) || !extract_id_from_fixed_row(fixed_row, &id)) {
            fclose(file);
            set_error(error_message, error_size, "데이터 파일이 올바르지 않습니다");
            return -1;
        }

        stored_offset = bplus_tree_get(handle->tree, id);
        if (stored_offset < 0 || decode_offset_from_index(stored_offset) != offset) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

/* 5.4 위치 기반 읽기/쓰기: 지정한 byte offset에서 fixed row 하나를 읽는다. */
static int read_data_row(FILE *file, const TableMetadata *table, long offset, char fixed_row[ROW_SIZE]) {
    if (fseek(file, offset, SEEK_SET) != 0) {
        return 0;
    }

    if (fread(fixed_row, 1, table->row_size, file) != (size_t) table->row_size) {
        return 0;
    }

    return fixed_row[ROW_DATA_SIZE] == '\n';
}

/* 5.3 논리 row 변환: fixed row에서 padding을 제거하고 첫 번째 컬럼 id를 추출한다. */
static int extract_id_from_fixed_row(const char fixed_row[ROW_SIZE], int *id) {
    char logical_row[ROW_SIZE];
    int end = ROW_DATA_SIZE - 1;
    char *comma;

    if (fixed_row[ROW_DATA_SIZE] != '\n') {
        return 0;
    }

    memcpy(logical_row, fixed_row, ROW_DATA_SIZE);
    logical_row[ROW_DATA_SIZE] = '\0';

    while (end >= 0 && logical_row[end] == ROW_PADDING_CHAR) {
        logical_row[end] = '\0';
        end--;
    }

    if (end < 0 || logical_row[end] != ',') {
        return 0;
    }
    logical_row[end] = '\0';

    comma = strchr(logical_row, ',');
    if (comma != NULL) {
        *comma = '\0';
    }

    return parse_int_text(logical_row, id);
}

/* 내부 구현: 테이블 이름에 해당하는 열린 인덱스 핸들을 찾는다. */
static IndexHandle *find_index_handle(const char *table_name) {
    for (int i = 0; i < INDEX_HANDLE_COUNT; i++) {
        if (strcmp(INDEX_HANDLES[i].table->name, table_name) == 0) {
            return &INDEX_HANDLES[i];
        }
    }

    return NULL;
}

/* 내부 구현: 테이블 인덱스 핸들을 재사용하거나 새 슬롯에 등록한다. */
static IndexHandle *ensure_index_handle(const TableMetadata *table) {
    IndexHandle *handle = find_index_handle(table->name);

    if (handle != NULL) {
        return handle;
    }

    if (INDEX_HANDLE_COUNT >= MAX_VALUES) {
        return NULL;
    }

    handle = &INDEX_HANDLES[INDEX_HANDLE_COUNT];
    handle->table = table;
    handle->tree = NULL;
    INDEX_HANDLE_COUNT++;
    return handle;
}

/* 내부 구현: 빈 테이블도 시작할 수 있도록 데이터 파일을 준비한다. */
static int ensure_data_file(const TableMetadata *table, char *error_message, size_t error_size) {
    FILE *file = fopen(table->csv_file_path, "ab");

    if (file == NULL) {
        set_error(error_message, error_size, "CSV 파일을 열 수 없습니다");
        return 0;
    }

    fclose(file);
    return 1;
}

/* 내부 구현: thirdparty B+Tree 파일 핸들을 연다. */
static int init_tree(IndexHandle *handle, char *error_message, size_t error_size) {
    handle->tree = bplus_tree_init((char *) handle->table->index_file_path, BPLUS_BLOCK_SIZE);
    if (handle->tree == NULL) {
        set_error(error_message, error_size, "인덱스를 준비할 수 없습니다");
        return 0;
    }

    return 1;
}

/* 내부 구현: thirdparty B+Tree가 사용하는 본 파일과 boot 파일이 모두 있는지 확인한다. */
static int index_files_exist(const TableMetadata *table) {
    char boot_path[MAX_INPUT_SIZE];
    FILE *index_file;
    FILE *boot_file;

    snprintf(boot_path, sizeof(boot_path), "%s.boot", table->index_file_path);
    index_file = fopen(table->index_file_path, "rb");
    boot_file = fopen(boot_path, "rb");

    if (index_file != NULL) {
        fclose(index_file);
    }
    if (boot_file != NULL) {
        fclose(boot_file);
    }

    return index_file != NULL && boot_file != NULL;
}

/* 내부 구현: 복구 전에 기존 B+Tree 파일 묶음을 지운다. */
static void remove_index_files(const TableMetadata *table) {
    char boot_path[MAX_INPUT_SIZE];

    snprintf(boot_path, sizeof(boot_path), "%s.boot", table->index_file_path);
    remove(table->index_file_path);
    remove(boot_path);
}

/* 내부 구현: id 문자열을 B+Tree key로 쓸 정수로 변환한다. */
static int parse_int_text(const char *text, int *value) {
    char *end;
    long parsed;

    if (text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (*end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *value = (int) parsed;
    return 1;
}

/* 내부 구현: 현재 파일 크기를 byte 단위로 얻는다. */
static long get_file_size(FILE *file) {
    long file_size;

    if (fseek(file, 0, SEEK_END) != 0) {
        return -1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        return -1;
    }

    return file_size;
}

/* 내부 구현: 라이브러리에서 0은 delete 의미이므로 실제 offset에 1을 더해 저장한다. */
static long encode_offset_for_index(long offset) {
    if (offset == LONG_MAX) {
        return -1;
    }

    return offset + 1;
}

/* 내부 구현: B+Tree에 저장한 offset + 1 값을 실제 byte offset으로 되돌린다. */
static long decode_offset_from_index(long stored_offset) {
    if (stored_offset <= 0) {
        return -1;
    }

    return stored_offset - 1;
}

/* 내부 구현: 호출자에게 전달할 오류 메시지를 고정 크기 버퍼에 복사한다. */
static void set_error(char *error_message, size_t error_size, const char *message) {
    if (error_size == 0) {
        return;
    }

    snprintf(error_message, error_size, "%s", message);
}

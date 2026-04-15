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

/* 파일 안에서만 사용: 테이블별 B+Tree 라이브러리 핸들을 관리하는 함수 목록이다. */
static IndexHandle *find_index_handle(const char *table_name);
static IndexHandle *ensure_index_handle(const TableMetadata *table);
static int ensure_data_file(const TableMetadata *table, char *error_message, size_t error_size);
static int init_tree(IndexHandle *handle, char *error_message, size_t error_size);
static int index_files_exist(const TableMetadata *table);
static void remove_index_files(const TableMetadata *table);
static int validate_index_against_data(IndexHandle *handle, char *error_message, size_t error_size);
static int rebuild_index_from_data_file(IndexHandle *handle, char *error_message, size_t error_size);
static int read_data_row(FILE *file, const TableMetadata *table, long offset, char fixed_row[ROW_SIZE]);
static int extract_id_from_fixed_row(const char fixed_row[ROW_SIZE], int *id);
static int parse_int_text(const char *text, int *value);
static long get_file_size(FILE *file);
static long encode_offset_for_index(long offset);
static long decode_offset_from_index(long stored_offset);
static void set_error(char *error_message, size_t error_size, const char *message);

static IndexHandle INDEX_HANDLES[MAX_VALUES];
static int INDEX_HANDLE_COUNT = 0;

/* 인덱스 준비: 외부 B+Tree 파일을 열고, 없거나 불일치하면 데이터 파일 기준으로 복구한다. */
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

/* 종료 정리: 외부 B+Tree가 boot 파일과 데이터 파일을 flush하도록 닫는다. */
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

/* B+Tree 조회: id에 연결된 fixed row byte offset을 반환한다. */
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

/* B+Tree 삽입: 라이브러리 특성상 실제 offset 대신 offset + 1을 저장한다. */
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

static IndexHandle *find_index_handle(const char *table_name) {
    for (int i = 0; i < INDEX_HANDLE_COUNT; i++) {
        if (strcmp(INDEX_HANDLES[i].table->name, table_name) == 0) {
            return &INDEX_HANDLES[i];
        }
    }

    return NULL;
}

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

static int ensure_data_file(const TableMetadata *table, char *error_message, size_t error_size) {
    FILE *file = fopen(table->csv_file_path, "ab");

    if (file == NULL) {
        set_error(error_message, error_size, "CSV 파일을 열 수 없습니다");
        return 0;
    }

    fclose(file);
    return 1;
}

static int init_tree(IndexHandle *handle, char *error_message, size_t error_size) {
    handle->tree = bplus_tree_init((char *) handle->table->index_file_path, BPLUS_BLOCK_SIZE);
    if (handle->tree == NULL) {
        set_error(error_message, error_size, "인덱스를 준비할 수 없습니다");
        return 0;
    }

    return 1;
}

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

static void remove_index_files(const TableMetadata *table) {
    char boot_path[MAX_INPUT_SIZE];

    snprintf(boot_path, sizeof(boot_path), "%s.boot", table->index_file_path);
    remove(table->index_file_path);
    remove(boot_path);
}

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

static int read_data_row(FILE *file, const TableMetadata *table, long offset, char fixed_row[ROW_SIZE]) {
    if (fseek(file, offset, SEEK_SET) != 0) {
        return 0;
    }

    if (fread(fixed_row, 1, table->row_size, file) != (size_t) table->row_size) {
        return 0;
    }

    return fixed_row[ROW_DATA_SIZE] == '\n';
}

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

static long encode_offset_for_index(long offset) {
    if (offset == LONG_MAX) {
        return -1;
    }

    return offset + 1;
}

static long decode_offset_from_index(long stored_offset) {
    if (stored_offset <= 0) {
        return -1;
    }

    return stored_offset - 1;
}

static void set_error(char *error_message, size_t error_size, const char *message) {
    if (error_size == 0) {
        return;
    }

    snprintf(error_message, error_size, "%s", message);
}

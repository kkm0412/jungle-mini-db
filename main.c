#include <stdio.h>
#include <string.h>

#include "mini_db.h"

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

#define PROJECT_CSV_PATH(path) PROJECT_ROOT_DIR "/" path

/* 파일 안에서만 사용: 아래 핵심 함수들이 호출하는 내부 함수 목록이다. */
static int run_repl(void);
static char *trim(char *text);

/* 고정 데이터: 4.1 테이블 이름 매핑에서 사용하는 테이블 메타데이터다. */
static const TableMetadata GLOBAL_TABLES[] = {
    {"users", {"id", "name"}, 2, PROJECT_CSV_PATH("data/users.csv")},
    {"posts", {"id", "title"}, 2, PROJECT_CSV_PATH("data/posts.csv")},
};

static const int GLOBAL_TABLE_COUNT = sizeof(GLOBAL_TABLES) / sizeof(GLOBAL_TABLES[0]);

int main(void) {
    /* 흐름: 프로그램 시작점을 1. REPL SQL 입력 처리로 연결한다. */
    return run_repl();
}

/* 1. REPL SQL 입력 처리: SQL 한 줄을 받아 파싱과 실행으로 넘긴다. */
static int run_repl(void) {
    char input[MAX_INPUT_SIZE];

    while (1) {
        char *sql;
        Plan plan;

        /* 흐름: 1.1 프롬프트 출력 -> 1.2 SQL 한 줄 읽기 */
        printf("mini-db> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            return 0;
        }

        sql = trim(input);

        /* 흐름: 1.2 SQL 한 줄 읽기 -> 1.3 특수 명령 처리 */
        if (strcmp(sql, ".exit") == 0) {
            return 0;
        }

        /* 흐름: 1.3 특수 명령 처리 -> 2.1 SQL 타입 판별 */
        plan = parse_sql(sql);
        if (plan.type == QUERY_INVALID) {
            printf("%s\n", plan.error_message);
            continue;
        }

        /* 흐름: 2. SQL 파싱 결과 -> 3.1 실행 분기 */
        execute_plan(&plan);
    }
}

/* 4.1 테이블 이름 매핑: 테이블 이름을 컬럼 정보와 CSV 파일 경로로 바꾼다. */
const TableMetadata *find_table(const char *table_name) {
    for (int i = 0; i < GLOBAL_TABLE_COUNT; i++) {
        if (strcmp(GLOBAL_TABLES[i].name, table_name) == 0) {
            return &GLOBAL_TABLES[i];
        }
    }

    return NULL;
}

/* 내부 처리: 입력 문자열의 앞뒤 공백과 개행을 제거한다. */
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

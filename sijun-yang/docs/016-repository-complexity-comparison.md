# C 프로젝트 파일 길이와 복잡도 비교

이 문서는 `sijun-yang` 프로젝트를 기준으로, 외부 B-Tree / B+Tree SQL 프로젝트들의 `.c`, `.h` 파일 길이와 정적 복잡도를 비교한다.

측정일은 2026-04-15이다.

## 1. 비교 대상

| 구분 | 기준 위치 | 브랜치 / 커밋 |
|---|---|---|
| 내 프로젝트 | 로컬 `sijun-yang/` | `main` / `7a19862` |
| Wish-Upon-A-Star/SQL-B-Tree | <https://github.com/Wish-Upon-A-Star/SQL-B-Tree> | `main` / `d1e33fd` |
| KYUJEONGLEE/-WEEK-7-B-tree-index-db | <https://github.com/KYUJEONGLEE/-WEEK-7-B-tree-index-db/tree/dev> | `dev` / `e3dca09` |
| whiskend/BPTree_SQL_Engine | <https://github.com/whiskend/BPTree_SQL_Engine/tree/main> | `main` / `d08dfd0` |
| LJH098/week7_index | <https://github.com/LJH098/week7_index> | `main` / `6333485` |

## 2. 평가 기준

측정 대상은 각 프로젝트의 `.c`, `.h` 파일이다. `.git`, `.github`, `build`, `cmake-build*`, `dist`, `target`, `node_modules` 같은 저장소 메타데이터와 빌드 산출물은 제외했다.

복잡도는 완전한 C 파서가 아니라 정적 텍스트 분석으로 추정했다.

- 파일 길이: 파일별 실제 줄 수
- 함수 수: `.c` 파일에서 함수 정의 형태를 정규식으로 추정
- CC 합계: 함수별 추정 순환 복잡도 합계
- 평균 CC: `CC 합계 / 함수 수`
- 최대 CC: 단일 함수에서 가장 높은 추정 순환 복잡도
- CC 계산식: 함수 기본값 `1`에 `if`, `for`, `while`, `case`, `&&`, `||`, `?` 출현 수를 더함

전체 기준은 테스트, 벤치마크, 도구 파일까지 포함한다. 핵심 구현 기준은 `tests/`, `bench/`, `tools/`와 `test_`, `bench_`, `benchmark_`로 시작하는 파일을 제외한다.

## 3. 전체 `.c/.h` 기준 결과

| 레포 | 파일(.c/.h) | .c 줄 | .h 줄 | 총 줄 | 함수 | CC 합계 | 평균 CC | 최대 CC |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `sijun-yang/jungle-mini-db` | 7 (5/2) | 2,466 | 205 | 2,671 | 116 | 420 | 3.62 | 12 (`parser.c:read_integer`) |
| `Wish-Upon-A-Star/SQL-B-Tree` | 13 (8/5) | 7,163 | 277 | 7,440 | 240 | 2,252 | 9.38 | 97 (`executor.c:load_table_parse_snapshot`) |
| `KYUJEONGLEE/-WEEK-7-B-tree-index-db` | 22 (15/7) | 6,613 | 452 | 7,065 | 165 | 1,056 | 6.40 | 39 (`tests/test_storage.c:main`) |
| `whiskend/BPTree_SQL_Engine` | 32 (19/13) | 6,073 | 374 | 6,447 | 219 | 1,069 | 4.88 | 22 (`src/bptree.c:validate_node`) |
| `LJH098/week7_index` | 17 (11/6) | 4,351 | 365 | 4,716 | 104 | 705 | 6.78 | 38 (`src/tokenizer.c:tokenizer_tokenize_sql`) |

전체 기준으로 보면 내 프로젝트는 가장 작다. 줄 수는 `Wish-Upon-A-Star/SQL-B-Tree`의 약 36%, `KYUJEONGLEE`의 약 38%, `whiskend`의 약 41%, `LJH098`의 약 57% 수준이다.

복잡도 합계도 내 프로젝트가 가장 낮다. `Wish-Upon-A-Star/SQL-B-Tree`는 내 프로젝트보다 CC 합계가 약 5.36배 높고, 평균 CC도 9.38로 가장 높다. 특히 `executor.c` 하나에 구현과 분기 처리가 크게 집중되어 있다.

## 4. 핵심 구현 기준 결과

| 레포 | 핵심 파일(.c/.h) | 핵심 총 줄 | 핵심 함수 | 핵심 CC 합계 | 핵심 평균 CC | 핵심 최대 CC |
|---|---:|---:|---:|---:|---:|---:|
| `sijun-yang/jungle-mini-db` | 7 (5/2) | 2,671 | 116 | 420 | 3.62 | 12 (`parser.c:read_integer`) |
| `Wish-Upon-A-Star/SQL-B-Tree` | 10 (5/5) | 6,216 | 200 | 1,953 | 9.77 | 97 (`executor.c:load_table_parse_snapshot`) |
| `KYUJEONGLEE/-WEEK-7-B-tree-index-db` | 15 (8/7) | 6,048 | 137 | 845 | 6.17 | 38 (`src/tokenizer.c:tokenizer_tokenize_sql`) |
| `whiskend/BPTree_SQL_Engine` | 25 (12/13) | 4,845 | 134 | 893 | 6.66 | 22 (`src/bptree.c:validate_node`) |
| `LJH098/week7_index` | 13 (7/6) | 4,263 | 91 | 604 | 6.64 | 38 (`src/tokenizer.c:tokenizer_tokenize_sql`) |

핵심 구현 기준에서도 내 프로젝트는 규모와 복잡도가 가장 낮다. `whiskend/BPTree_SQL_Engine`은 파일 수가 가장 많지만 평균 CC는 `KYUJEONGLEE`, `LJH098`와 비슷한 수준이고, 최대 CC는 22로 낮은 편이다. 파일을 많이 나누어 구조화한 효과가 있다.

`KYUJEONGLEE`와 `LJH098`는 `tokenizer.c`, `utils.c`, `parser.c`, `storage.c`의 구조가 유사하고, 핵심 최대 CC가 모두 `tokenizer_tokenize_sql`에서 나온다. `KYUJEONGLEE`는 `executor.c`와 테스트/벤치마크가 더 커서 전체 규모가 더 크다.

## 5. 파일별 데이터

### 5.1 sijun-yang/jungle-mini-db

| 파일 | 줄 수 | 함수 | CC 합계 | 최대 CC |
|---|---:|---:|---:|---:|
| `db_index.c` | 400 | 19 | 83 | 11 (`rebuild_index_from_data_file`) |
| `executor.c` | 286 | 12 | 57 | 8 (`execute_insert`) |
| `main.c` | 135 | 7 | 26 | 11 (`trim`) |
| `mini_db.h` | 69 | 0 | 0 | 0 |
| `parser.c` | 402 | 20 | 93 | 12 (`read_integer`) |
| `thirdparty/bplustree.c` | 1,243 | 58 | 161 | 11 (`bplus_tree_get_range`) |
| `thirdparty/bplustree.h` | 136 | 0 | 0 | 0 |

### 5.2 Wish-Upon-A-Star/SQL-B-Tree

| 파일 | 줄 수 | 함수 | CC 합계 | 최대 CC |
|---|---:|---:|---:|---:|
| `bench_formula_test.c` | 59 | 3 | 9 | 4 (`normalize`) |
| `bench_workload_generator.c` | 434 | 17 | 104 | 24 (`generate_sqls`) |
| `benchmark_runner.c` | 731 | 20 | 186 | 77 (`main`) |
| `bptree.c` | 911 | 49 | 249 | 28 (`bptree_string_build_from_sorted`) |
| `bptree.h` | 44 | 0 | 0 | 0 |
| `executor.c` | 4,448 | 127 | 1,531 | 97 (`load_table_parse_snapshot`) |
| `executor.h` | 15 | 0 | 0 | 0 |
| `lexer.c` | 103 | 2 | 43 | 39 (`get_next_token`) |
| `lexer.h` | 13 | 0 | 0 | 0 |
| `main.c` | 233 | 9 | 63 | 28 (`main`) |
| `parser.c` | 244 | 13 | 67 | 10 (`parse_insert`) |
| `parser.h` | 11 | 0 | 0 | 0 |
| `types.h` | 194 | 0 | 0 | 0 |

### 5.3 KYUJEONGLEE/-WEEK-7-B-tree-index-db

| 파일 | 줄 수 | 함수 | CC 합계 | 최대 CC |
|---|---:|---:|---:|---:|
| `bench/benchmark.c` | 234 | 8 | 37 | 13 (`main`) |
| `src/bptree.c` | 548 | 19 | 81 | 12 (`bptree_insert_into_internal_after_splitting`) |
| `src/bptree.h` | 49 | 0 | 0 | 0 |
| `src/executor.c` | 1,032 | 37 | 201 | 14 (`executor_collect_linear_rows`) |
| `src/executor.h` | 74 | 0 | 0 | 0 |
| `src/index.c` | 432 | 14 | 93 | 22 (`index_collect_win_count_row_indexes`) |
| `src/index.h` | 39 | 0 | 0 | 0 |
| `src/main.c` | 334 | 7 | 51 | 15 (`main_run_repl_mode`) |
| `src/parser.c` | 385 | 12 | 72 | 18 (`parser_parse_insert`) |
| `src/parser.h` | 54 | 0 | 0 | 0 |
| `src/storage.c` | 1,753 | 13 | 75 | 21 (`storage_parse_csv_line`) |
| `src/storage.h` | 70 | 0 | 0 | 0 |
| `src/tokenizer.c` | 585 | 16 | 118 | 38 (`tokenizer_tokenize_sql`) |
| `src/tokenizer.h` | 51 | 0 | 0 | 0 |
| `src/utils.c` | 527 | 19 | 154 | 26 (`utils_utf8_decode`) |
| `src/utils.h` | 115 | 0 | 0 | 0 |
| `tests/test_bptree.c` | 87 | 2 | 18 | 16 (`main`) |
| `tests/test_executor.c` | 169 | 5 | 27 | 22 (`main`) |
| `tests/test_index.c` | 104 | 4 | 28 | 17 (`main`) |
| `tests/test_parser.c` | 95 | 2 | 26 | 24 (`main`) |
| `tests/test_storage.c` | 231 | 5 | 46 | 39 (`main`) |
| `tests/test_tokenizer.c` | 97 | 2 | 29 | 27 (`main`) |

### 5.4 whiskend/BPTree_SQL_Engine

| 파일 | 줄 수 | 함수 | CC 합계 | 최대 CC |
|---|---:|---:|---:|---:|
| `include/ast.h` | 51 | 0 | 0 | 0 |
| `include/benchmark.h` | 24 | 0 | 0 | 0 |
| `include/bptree.h` | 37 | 0 | 0 | 0 |
| `include/cli.h` | 13 | 0 | 0 | 0 |
| `include/errors.h` | 16 | 0 | 0 | 0 |
| `include/executor.h` | 14 | 0 | 0 | 0 |
| `include/lexer.h` | 40 | 0 | 0 | 0 |
| `include/parser.h` | 14 | 0 | 0 | 0 |
| `include/result.h` | 32 | 0 | 0 | 0 |
| `include/runtime.h` | 53 | 0 | 0 | 0 |
| `include/schema.h` | 24 | 0 | 0 | 0 |
| `include/storage.h` | 45 | 0 | 0 | 0 |
| `include/utils.h` | 11 | 0 | 0 | 0 |
| `src/benchmark.c` | 286 | 8 | 36 | 12 (`run_benchmark`) |
| `src/bptree.c` | 597 | 16 | 121 | 22 (`validate_node`) |
| `src/cli.c` | 66 | 3 | 16 | 14 (`parse_cli_args`) |
| `src/executor.c` | 896 | 21 | 173 | 18 (`build_insert_row_existing_behavior`) |
| `src/lexer.c` | 334 | 14 | 85 | 17 (`token_type_name`) |
| `src/main.c` | 120 | 2 | 20 | 17 (`main`) |
| `src/parser.c` | 484 | 21 | 90 | 14 (`parse_next_statement`) |
| `src/result.c` | 89 | 3 | 20 | 14 (`print_exec_result`) |
| `src/runtime.c` | 340 | 10 | 74 | 16 (`get_or_load_table_runtime`) |
| `src/schema.c` | 312 | 10 | 66 | 21 (`load_table_schema`) |
| `src/storage.c` | 845 | 22 | 172 | 21 (`append_row_to_table_with_offset`) |
| `src/utils.c` | 102 | 4 | 20 | 8 (`read_text_file`) |
| `tests/test_bptree.c` | 165 | 9 | 22 | 4 (`test_root_and_internal_split`) |
| `tests/test_executor.c` | 467 | 20 | 45 | 7 (`read_entire_file`) |
| `tests/test_lexer.c` | 130 | 10 | 14 | 2 (`assert_int_eq`) |
| `tests/test_parser.c` | 189 | 14 | 24 | 3 (`test_parse_next_statement_handles_multiple_sqls`) |
| `tests/test_runtime_index.c` | 235 | 13 | 19 | 3 (`ensure_directory`) |
| `tests/test_storage.c` | 334 | 16 | 30 | 6 (`read_entire_file`) |
| `tools/benchmark_bptree.c` | 82 | 3 | 22 | 15 (`main`) |

### 5.5 LJH098/week7_index

| 파일 | 줄 수 | 함수 | CC 합계 | 최대 CC |
|---|---:|---:|---:|---:|
| `src/executor.c` | 454 | 15 | 84 | 16 (`executor_collect_indexed_rows`) |
| `src/executor.h` | 12 | 0 | 0 | 0 |
| `src/index.c` | 395 | 12 | 73 | 19 (`index_query_range`) |
| `src/index.h` | 64 | 0 | 0 | 0 |
| `src/main.c` | 254 | 7 | 45 | 15 (`main_run_repl_mode`) |
| `src/parser.c` | 385 | 12 | 72 | 18 (`parser_parse_insert`) |
| `src/parser.h` | 54 | 0 | 0 | 0 |
| `src/storage.c` | 1,298 | 10 | 58 | 21 (`storage_parse_csv_line`) |
| `src/storage.h` | 69 | 0 | 0 | 0 |
| `src/tokenizer.c` | 585 | 16 | 118 | 38 (`tokenizer_tokenize_sql`) |
| `src/tokenizer.h` | 51 | 0 | 0 | 0 |
| `src/utils.c` | 527 | 19 | 154 | 26 (`utils_utf8_decode`) |
| `src/utils.h` | 115 | 0 | 0 | 0 |
| `tests/test_executor.c` | 106 | 5 | 14 | 9 (`main`) |
| `tests/test_parser.c` | 95 | 2 | 26 | 24 (`main`) |
| `tests/test_storage.c` | 155 | 4 | 32 | 26 (`main`) |
| `tests/test_tokenizer.c` | 97 | 2 | 29 | 27 (`main`) |

## 6. 해석 요약

내 프로젝트는 전체 줄 수와 CC 합계가 가장 낮고, 평균 CC도 가장 낮다. 현재 구조는 기능 범위가 작고, `thirdparty/bplustree.c`를 제외하면 각 파일의 복잡도가 비교적 낮게 분산되어 있다.

`Wish-Upon-A-Star/SQL-B-Tree`는 파일 수는 많지 않지만 `executor.c`가 4,448줄이고 CC 합계가 1,531로 매우 크다. 단일 파일과 단일 함수에 복잡도가 집중된 구조라 유지보수 난도가 가장 높아 보인다.

`whiskend/BPTree_SQL_Engine`은 파일 수가 가장 많지만, 전체 평균 CC는 `Wish-Upon-A-Star`보다 낮다. 기능을 `src/`와 `include/`로 나누어 모듈화한 대신 파일 개수가 늘어난 형태다.

`KYUJEONGLEE`와 `LJH098`는 핵심 구현 방식이 비슷하지만, `KYUJEONGLEE` 쪽이 `executor.c`, `storage.c`, 테스트, 벤치마크까지 더 커서 전체 규모가 더 크다.

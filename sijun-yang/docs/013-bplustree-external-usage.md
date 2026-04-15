# B+Tree 외부 사용자를 위한 아주 간단한 사용법

`thirdparty/bplustree.c`와 `thirdparty/bplustree.h`는 정수 key와 `long` value를 저장하는 파일 기반 B+Tree 구현이다.

이 구현은 메모리 전용 map이 아니라 디스크에 index node를 저장한다. `bplus_tree_init()`에 넘긴 파일 이름을 기준으로 실제 index 파일과 boot 파일이 만들어지고, `bplus_tree_deinit()`을 호출하면 root offset 같은 메타데이터가 저장된다.

## B+Tree

`struct bplus_tree`는 정수 key로 `long` 값을 빠르게 찾기 위한 B+Tree index다.

key 타입은 `key_t`이고, 이 프로젝트에 포함된 헤더에서는 `int`로 정의되어 있다. value 타입은 `long`이다.

새 B+Tree를 만들거나 기존 B+Tree를 열려면 `bplus_tree_init()`을 사용한다.

key와 value를 저장하려면 `bplus_tree_put()`을 사용한다.

key로 value를 찾으려면 `bplus_tree_get()`을 사용한다.

트리 내용을 디버깅 목적으로 출력하려면 `bplus_tree_dump()`를 사용한다.

B+Tree 사용을 끝낼 때는 반드시 `bplus_tree_deinit()`을 호출한다.

## 기본 예제

다음 예제는 `example.idx` 파일에 `id -> file offset` 형태의 mapping을 저장한다.

```c
#include <stdio.h>
#include <sys/types.h>

#include "thirdparty/bplustree.h"

int main(void) {
    struct bplus_tree *tree;
    long value;

    tree = bplus_tree_init("example.idx", 4096);
    if (tree == NULL) {
        return 1;
    }

    /* key 10에 파일 위치 128을 연결한다. */
    if (bplus_tree_put(tree, 10, 128) != 0) {
        bplus_tree_deinit(tree);
        return 1;
    }

    value = bplus_tree_get(tree, 10);
    if (value >= 0) {
        printf("key=10 value=%ld\n", value);
    } else {
        printf("key=10 not found\n");
    }

    bplus_tree_deinit(tree);
    return 0;
}
```

컴파일 예시:

```sh
cc -std=c11 -D_POSIX_C_SOURCE=200809L -o example example.c thirdparty/bplustree.c
```

실행 후에는 다음 파일들이 생긴다.

```text
example.idx       실제 B+Tree node 저장 파일
example.idx.boot  root offset, block size, free block 정보 저장 파일
```

## 주요 함수

### `bplus_tree_init()`

```c
struct bplus_tree *bplus_tree_init(char *filename, int block_size);
```

`filename`으로 B+Tree index 파일을 열고, 없으면 새로 만든다.

`block_size`는 node 하나의 크기다. 반드시 2의 거듭제곱이어야 한다. 일반적으로 `4096`을 사용하면 된다.

성공하면 `struct bplus_tree *`를 반환하고, 실패하면 `NULL`을 반환한다.

### `bplus_tree_put()`

```c
int bplus_tree_put(struct bplus_tree *tree, key_t key, long data);
```

`key -> data` mapping을 추가하거나 갱신한다.

반환값이 `0`이면 성공이다.

주의할 점은 `data == 0`이 삭제 명령으로 처리된다는 것이다. 따라서 실제 value로 `0`을 저장하면 안 된다. 파일 offset `0`을 저장해야 한다면 외부 wrapper에서 `offset + 1`로 저장하고, 읽을 때 다시 `-1` 하는 방식이 필요하다.

### `bplus_tree_get()`

```c
long bplus_tree_get(struct bplus_tree *tree, key_t key);
```

`key`에 연결된 `data`를 반환한다.

key가 없으면 `-1`을 반환한다.

### `bplus_tree_get_range()`

```c
long bplus_tree_get_range(struct bplus_tree *tree, key_t key1, key_t key2);
```

`key1`과 `key2` 사이를 leaf 순서로 훑는다.

이 함수는 범위 안의 모든 값을 반환하는 iterator가 아니다. 현재 구현에서는 범위 안에서 마지막으로 확인한 `data` 하나만 반환한다. 여러 값을 모두 읽어야 하는 외부 API로 쓰기에는 적합하지 않다.

### `bplus_tree_dump()`

```c
void bplus_tree_dump(struct bplus_tree *tree);
```

B+Tree 내부 구조를 표준 출력에 출력한다.

디버깅용 함수로만 사용하는 것이 좋다.

### `bplus_tree_deinit()`

```c
void bplus_tree_deinit(struct bplus_tree *tree);
```

B+Tree의 메타데이터를 boot 파일에 저장하고, 열린 파일과 메모리를 정리한다.

프로그램 종료 전에 반드시 호출해야 한다.

## 삭제 예제

`bplus_tree_put()`에 `data`를 `0`으로 넘기면 해당 key를 삭제한다.

```c
if (bplus_tree_put(tree, 10, 0) != 0) {
    printf("delete failed\n");
}
```

삭제 후 다시 조회하면 `-1`이 반환된다.

```c
if (bplus_tree_get(tree, 10) < 0) {
    printf("key=10 not found\n");
}
```

## 사용할 때 기억할 점

- `thirdparty` 하위 코드는 외부에서 가져온 코드이므로 직접 수정하지 않는다.
- key는 `int` 범위 안에 있어야 한다.
- value는 `long`이지만 `0`은 삭제 의미라서 저장 값으로 쓰면 안 된다.
- `bplus_tree_deinit()`을 호출해야 index 메타데이터가 저장된다.
- 이 구현은 POSIX API인 `open`, `pread`, `pwrite`, `fsync`, `close`를 사용한다.
- 여러 B+Tree를 동시에 쓸 수는 있지만 내부에 block size 관련 전역 상태가 있으므로 같은 `block_size`로 초기화하는 편이 안전하다.
- 애플리케이션 코드에서는 가능하면 직접 호출하지 말고, 프로젝트의 `db_index.c`처럼 wrapper를 두고 사용하는 것이 좋다.

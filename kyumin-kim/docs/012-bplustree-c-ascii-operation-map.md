# bplustree.c 동작 ASCII 구조도

이 문서는 `thirdparty/bplustree.c`가 어떤 방식으로 B+Tree를 구현하는지 그림 중심으로 정리한다.

핵심은 다음과 같다.

- 이 구현은 메모리 전용 B+Tree가 아니라 파일 기반 B+Tree다.
- node 포인터 대신 파일 안의 byte offset인 `off_t`를 저장한다.
- internal node는 검색 경로를 안내한다.
- leaf node는 실제 data 값을 저장한다.
- leaf node끼리는 `prev` / `next` offset으로 연결된다.
- 우리 mini DB에서는 B+Tree의 `data` 값으로 CSV row offset을 저장한다.

## 1. 전체 파일 구성

테이블 하나에 대해 B+Tree는 두 파일을 사용한다.

```text
data/posts.idx
└── B+Tree node block들이 저장되는 본문 파일

data/posts.idx.boot
└── root offset, block size, file size, free block 목록을 저장하는 boot 파일
```

프로그램 시작 시:

```text
db_index_open_table()
  -> init_tree()
      -> bplus_tree_init("data/posts.idx", 4096)
```

프로그램 종료 시:

```text
db_index_shutdown_all()
  -> bplus_tree_deinit()
      -> root/file_size/free_blocks를 .boot 파일에 저장
```

## 2. node는 포인터가 아니라 offset으로 연결된다

일반적인 메모리 B+Tree는 child를 포인터로 연결한다.

```text
parent
  children[0] -> child pointer
  children[1] -> child pointer
```

이 구현은 파일 기반이라 child 위치를 파일 offset으로 저장한다.

```text
parent node in data/posts.idx
  sub[0] = 0
  sub[1] = 4096
  sub[2] = 8192

파일 offset 0    -> child node A
파일 offset 4096 -> child node B
파일 offset 8192 -> child node C
```

노드를 읽을 때:

```text
node_seek(tree, offset)
  -> pread(tree->fd, cache_buffer, block_size, offset)
  -> 파일 offset 위치의 block 하나를 메모리 cache에 읽음
```

노드를 저장할 때:

```text
node_flush(tree, node)
  -> pwrite(tree->fd, node, block_size, node->self)
  -> node->self offset 위치에 block 하나를 다시 씀
```

## 3. node block 내부 배치

`struct bplus_node`에는 공통 header만 있다.

```text
struct bplus_node
  self
  parent
  prev
  next
  type
  children
```

key, data, child offset 배열은 구조체 필드로 직접 선언되어 있지 않고, block 뒤쪽 메모리를 매크로로 해석한다.

```text
leaf node block

+----------------------+--------------------+--------------------+
| bplus_node header    | key array          | data array         |
| self/parent/prev/... | key[0], key[1] ... | data[0], data[1]...|
+----------------------+--------------------+--------------------+
```

```text
internal node block

+----------------------+--------------------+--------------------+
| bplus_node header    | key array          | sub offset array   |
| self/parent/prev/... | key[0], key[1] ... | sub[0], sub[1] ... |
+----------------------+--------------------+--------------------+
```

코드에서는 이 부분이 다음 매크로로 보인다.

```c
key(node)
data(node)
sub(node)
```

## 4. mini DB에서 저장하는 값

mini DB의 실제 row는 CSV 파일에 있다.

```text
data/posts.csv

offset 0    -> 1,welcome,...
offset 64   -> 2,btree-index,...
offset 128  -> 3,leaf-scan,...
offset 192  -> 4,range-query,...
```

B+Tree는 row 내용이 아니라 `id -> row offset`만 저장한다.

```text
B+Tree leaf data

key:   1    2    3    4
data:  1    65   129  193
```

주의할 점:

```text
실제 CSV offset 0   -> B+Tree에는 1 저장
실제 CSV offset 64  -> B+Tree에는 65 저장
실제 CSV offset 128 -> B+Tree에는 129 저장
```

이유는 `bplus_tree_put(tree, key, 0)`이 삭제(delete) 의미이기 때문이다.

```text
db_index.c
  저장할 때: actual_offset + 1
  읽을 때:   stored_offset - 1
```

## 5. 검색 흐름

예를 들어 `id = 7`을 찾는다고 하자.

```text
             root internal
             keys: [5 | 9]
              /     |     \
             /      |      \
            v       v       v
      leaf [1 2 3 4] leaf [5 6 7 8] leaf [9 10 11]
                      data for 7 찾음
```

실제 코드 흐름:

```text
bplus_tree_get(tree, 7)
  -> bplus_tree_search(tree, 7)
      -> node_seek(tree, tree->root)
      -> internal node에서 key_binary_search()
      -> 알맞은 sub[offset]으로 node_seek()
      -> leaf에 도착
      -> leaf의 key 배열에서 7 찾기
      -> data 배열에서 같은 index의 value 반환
```

internal node에서 내려가는 규칙:

```text
key를 찾았으면     sub[i + 1]로 이동
key를 못 찾았으면  들어갈 위치 i를 계산해서 sub[i]로 이동
```

leaf node에서 찾으면:

```text
key[2]  = 7
data[2] = 385

return 385
```

mini DB wrapper는 이 값을 다시 CSV offset으로 바꾼다.

```text
stored data 385
-> decode_offset_from_index()
-> actual CSV offset 384
```

## 6. 삽입 흐름

`id = 8`, `offset = 448`을 삽입한다고 하자.

mini DB에서는 먼저 offset을 B+Tree용 값으로 바꾼다.

```text
actual offset = 448
stored data   = 449
```

그 다음:

```text
db_index_put("posts", 8, location)
  -> bplus_tree_put(tree, 8, 449)
      -> bplus_tree_insert(tree, 8, 449)
```

삽입은 검색과 비슷하게 leaf까지 내려간다.

```text
root
  -> internal key 비교
  -> sub offset으로 이동
  -> leaf 도착
  -> 정렬 위치에 key/data 삽입
```

leaf에 공간이 있으면 단순 삽입이다.

```text
before
leaf keys: [5 6 7]

insert 8

after
leaf keys: [5 6 7 8]
```

## 7. leaf split 흐름

leaf가 꽉 찼는데 새 key가 들어오면 split한다.

예시 그림:

```text
before

leaf A
keys: [1 2 3 4]
```

split 후:

```text
after

leaf A             leaf B
keys: [1 2]  ->    keys: [3 4]
next: B            next: old_next
```

부모로 올라가는 key는 오른쪽 leaf의 첫 key다.

```text
promoted key = 3
```

B+Tree에서는 이 key가 부모로 복사되고, leaf에도 그대로 남는다.

```text
        parent
        keys: [3]
        /       \
       v         v
  leaf [1 2] -> leaf [3 4]
```

이것이 B-Tree와 다른 지점이다.

```text
B+Tree
  실제 data는 leaf에 남는다.
  parent key는 길 안내용 복사본이다.

B-Tree
  중간 key가 위로 이동하고, data가 internal node에 있을 수도 있다.
```

## 8. internal split 흐름

internal node도 꽉 차면 split한다.

```text
before

internal
keys: [3 5 7 9]
children: C0 C1 C2 C3 C4
```

가운데 key를 부모로 올린다.

```text
promoted key = 7
```

split 후:

```text
left internal       right internal
keys: [3 5]         keys: [9]
children: C0 C1 C2  children: C3 C4
```

부모에서 보는 모습:

```text
            parent
            keys: [7]
             /     \
            v       v
     internal[3 5]  internal[9]
```

internal split에서는 promoted key가 child node 안에 남지 않는다.

## 9. root split 흐름

root가 split되면 새 root를 만든다.

처음에는 root가 leaf일 수 있다.

```text
root leaf
keys: [1 2 3 4]
```

root leaf가 split되면:

```text
leaf [1 2] -> leaf [3 4]
promoted key = 3
```

부모가 없으므로 새 root를 만든다.

```text
tree->root
   |
   v
internal root
keys: [3]
children:
  0 -> leaf [1 2]
  1 -> leaf [3 4]
```

그래서 `tree->root = new_root`는 B-Tree라는 뜻이 아니다.

```text
새 root = 길 안내용 internal node
실제 data = 여전히 leaf node
```

## 10. leaf linked list와 range scan

leaf node는 `next`로 이어져 있다.

```text
leaf [1 2] -> leaf [3 4] -> leaf [5 6] -> leaf [7 8]
```

범위 조회는 다음 순서로 동작한다.

```text
select * from posts where id between 3 and 7;
```

```text
1. root에서 id 3이 들어갈 leaf까지 내려간다.

             root
            [5]
           /   \
          v     v
   leaf [1 2 3 4] -> leaf [5 6 7 8]

2. leaf 안에서 3의 위치를 찾는다.

   leaf [1 2 3 4]
             ^
             start

3. leaf 끝까지 읽는다.

   3, 4

4. 아직 end key 7까지 못 갔으므로 next leaf로 간다.

   node = node_seek(tree, node->next)

5. 다음 leaf에서 7까지 읽고 멈춘다.

   leaf [5 6 7 8]
        5 6 7 stop
```

최종 결과:

```text
3, 4, 5, 6, 7
```

우리 프로젝트에서 추가한 `bplus_tree_scan_leafs_from()`도 같은 `next` 연결을 사용한다.

```text
bplus_tree_scan_leafs_from(tree, start_key, leaf_count, values, max_values)

root에서 start_key leaf 찾기
  -> 현재 leaf의 data 읽기
  -> 부족하면 node->next로 다음 leaf 이동
  -> leaf_count 또는 max_values에 도달하면 종료
```

핵심 코드 모양:

```c
node = node_seek(tree, node->next);
```

## 11. 현재 구현의 특징 요약

```text
좋은 점
  - 실제 파일 기반 B+Tree 구조를 볼 수 있다.
  - node가 block 단위로 저장된다.
  - leaf next 링크가 있어 range scan에 적합하다.
  - insert/delete 시 split, merge, borrow 흐름이 있다.

어려운 점
  - key/data/sub 배열이 struct 필드가 아니라 매크로로 계산된다.
  - node 포인터가 아니라 파일 offset을 따라간다.
  - cache를 직접 빌리고 반납한다.
  - .idx와 .idx.boot 파일이 분리되어 있다.
  - bplus_tree_get_range()는 이름과 달리 목록을 반환하지 않는다.

mini DB에서의 역할
  - id를 key로 사용한다.
  - CSV row 위치를 data로 저장한다.
  - SELECT id 조건 조회는 B+Tree에서 offset을 찾고 CSV로 이동한다.
  - SELECT id between 범위 조회는 leaf next 링크를 따라 여러 row 위치를 얻는다.
```

## 12. 한 줄 요약

```text
bplustree.c는 "포인터 기반 교과서 B+Tree"가 아니라
"파일 offset 기반 B+Tree index"다.

internal node는 길을 찾고,
leaf node는 id -> CSV offset을 저장하고,
leaf next 링크는 범위 조회를 가능하게 한다.
```

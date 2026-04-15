# B-Tree 개념과 `thirdparty/bplustree.c` 구현 연결

이 문서는 `docs/012-btree-script-summary.md`에서 설명한 B-Tree 개념이 실제 `thirdparty/bplustree.c`의 B+Tree 구현에서 어떻게 나타나는지 연결해서 설명한다.

`012` 문서는 개념 중심이다. 이 문서는 코드 중심이다.

핵심 차이는 하나다.

- B-Tree 개념: internal node와 leaf node 모두 key와 값을 가질 수 있는 균형 tree로 설명한다.
- 이 구현의 B+Tree: 실제 `data`는 leaf node에만 저장하고, non-leaf node는 검색 경로를 고르는 separator key와 child offset만 가진다.

따라서 `012`의 B-Tree 규칙을 읽을 때는 다음처럼 바꿔 생각하면 된다.

```text
B-Tree의 node key
-> B+Tree에서도 key 배열로 유지

B-Tree의 child pointer
-> 이 구현에서는 파일 안 node 위치인 off_t offset

B-Tree의 node 접근 비용
-> 이 구현에서는 pread/pwrite로 block 단위 node를 읽고 쓰는 비용

B-Tree의 leaf 균형
-> 이 구현에서도 모든 leaf는 같은 level에 있고, leaf끼리는 prev/next로 연결됨

B-Tree의 key-value 저장
-> 이 구현에서는 leaf node의 key[]와 data[]에만 저장
```

## 1. node 하나는 파일 block 하나다

`012`에서는 node를 메모리상의 구조처럼 설명한다. `thirdparty/bplustree.c`에서는 node 하나가 index 파일 안의 고정 크기 block이다.

관련 전역 설정은 다음 세 가지다.

```c
static int _block_size;
static int _max_entries;
static int _max_order;
```

`bplus_tree_init()`은 `block_size`를 기준으로 node 하나에 들어갈 수 있는 key 수를 계산한다.

```text
_max_order
-> non-leaf node가 가질 수 있는 child branch 수
-> non-leaf key 수는 최대 _max_order - 1

_max_entries
-> leaf node가 가질 수 있는 key/data entry 수
```

즉 `012`에서 말한 "node에는 최대 key 수가 있다"는 규칙은 이 구현에서 `_max_order`, `_max_entries`로 구체화된다.

node의 실제 파일 위치는 `off_t` offset이다.

```c
typedef struct bplus_node {
        off_t self;
        off_t parent;
        off_t prev;
        off_t next;
        int type;
        int children;
} bplus_node;
```

각 필드의 의미는 다음과 같다.

- `self`: 이 node가 index 파일 안에서 시작되는 offset
- `parent`: parent node의 offset
- `prev`, `next`: 같은 level의 왼쪽/오른쪽 sibling offset
- `type`: leaf인지 non-leaf인지 구분
- `children`: leaf에서는 entry 수, non-leaf에서는 child branch 수

여기서 주의할 점은 `children`의 의미가 node 종류에 따라 다르다는 것이다.

```text
leaf node:
children == key/data entry 개수

non-leaf node:
children == child branch 개수
key 개수 == children - 1
```

## 2. node 안의 배열은 pointer 계산으로 만든다

`struct bplus_node`에는 `key[]`, `data[]`, `sub[]` 배열 필드가 직접 선언되어 있지 않다. 대신 block 내부에서 header 뒤쪽 메모리를 배열처럼 해석한다.

```c
#define offset_ptr(node) ((char *) (node) + sizeof(*node))
#define key(node) ((key_t *)offset_ptr(node))
#define data(node) ((long *)(offset_ptr(node) + _max_entries * sizeof(key_t)))
#define sub(node) ((off_t *)(offset_ptr(node) + (_max_order - 1) * sizeof(key_t)))
```

이 구현에서 leaf와 non-leaf의 저장 구조는 다음처럼 다르다.

```text
leaf node:
[bplus_node header][key array][data array]

non-leaf node:
[bplus_node header][key array][sub offset array]
```

`012`의 B-Tree 설명에서는 "node 안에 여러 key가 있고, key 사이마다 child가 있다"고 했다. 이 구현에서는 그 관계가 `key(node)`와 `sub(node)` 배열로 표현된다.

예를 들어 non-leaf node가 다음과 같다고 하자.

```text
key: [10 | 20 | 30]
sub: [A  | B  | C  | D]
```

검색 범위는 다음과 같이 나뉜다.

```text
key < 10        -> sub[0]
10 <= key < 20  -> sub[1]
20 <= key < 30  -> sub[2]
30 <= key       -> sub[3]
```

B+Tree에서는 internal key가 실제 data가 아니라 separator다. 그래서 internal node에서 key가 정확히 일치해도 검색은 끝나지 않고 오른쪽 child로 내려간다.

## 3. node 접근 비용은 `pread`와 `pwrite`다

`012`에서는 B-Tree가 데이터베이스와 파일 시스템에 유리한 이유를 "비싼 node 접근 횟수를 줄이기 때문"이라고 설명한다.

이 구현에서 그 비싼 node 접근은 실제 파일 I/O다.

```c
static struct bplus_node *node_fetch(struct bplus_tree *tree, off_t offset)
static struct bplus_node *node_seek(struct bplus_tree *tree, off_t offset)
static inline void node_flush(struct bplus_tree *tree, struct bplus_node *node)
```

각 함수의 역할은 다음과 같다.

- `node_fetch()`: 특정 offset의 node를 cache buffer로 읽고, 나중에 `node_flush()`로 반납할 수 있게 used 표시를 한다.
- `node_seek()`: 특정 offset의 node를 임시로 읽는다. 검색 경로를 따라 내려갈 때 주로 사용한다.
- `node_flush()`: node 내용을 index 파일에 `pwrite()`하고 cache buffer를 반납한다.

즉 `012`의 "새 node를 가져오는 작업이 비싸다"는 설명은 이 코드에서는 `pread(tree->fd, node, _block_size, offset)` 호출로 드러난다.

B+Tree가 node 하나에 많은 key를 넣는 이유도 여기서 분명해진다.

```text
한 block에 많은 key 저장
-> 한 번 pread로 많은 separator를 확인
-> tree height 감소
-> 전체 pread 횟수 감소
```

## 4. 검색: key 비교 후 child offset을 고른다

`012`의 검색 과정은 다음이었다.

```text
root node에서 시작
-> node 안의 key와 비교
-> 같으면 끝, 아니면 해당 child로 내려감
-> leaf까지 반복
```

이 구현에서는 `bplus_tree_get()`이 `bplus_tree_search()`를 호출한다.

```c
long bplus_tree_get(struct bplus_tree *tree, key_t key)
{
        return bplus_tree_search(tree, key);
}
```

실제 검색 흐름은 다음과 같다.

```text
bplus_tree_get()
-> bplus_tree_search()
-> node_seek(tree, tree->root)
-> key_binary_search(node, key)
-> leaf이면 data 반환
-> non-leaf이면 sub[] offset을 골라 다음 node_seek()
```

`key_binary_search()`는 현재 node의 key 배열에서 target이 있는 위치를 찾는다.

- 찾으면 `0` 이상의 index를 반환한다.
- 없으면 `-insertion_position - 1` 형태의 음수를 반환한다.

leaf node에서는 key가 실제 entry이므로, 찾으면 `data(node)[i]`를 반환한다.

```text
leaf에서 key 발견
-> data(node)[i] 반환

leaf에서 key 없음
-> -1 반환
```

non-leaf node에서는 key가 separator일 뿐이다. 그래서 key가 같아도 검색이 끝나지 않는다.

```c
if (i >= 0) {
        node = node_seek(tree, sub(node)[i + 1]);
} else {
        i = -i - 1;
        node = node_seek(tree, sub(node)[i]);
}
```

이 부분이 B-Tree와 B+Tree의 차이를 가장 잘 보여준다.

```text
B-Tree 개념 설명:
internal node에서 key를 찾으면 값을 찾은 것으로 볼 수 있음

이 B+Tree 구현:
internal node의 key는 separator이므로 오른쪽 child로 내려감
실제 data는 leaf에서만 찾음
```

## 5. 삽입: 항상 leaf까지 내려간다

`012`에서는 "B-Tree에 새 key를 넣을 때는 항상 leaf level에 삽입한다"고 설명한다.

이 구현도 같다.

```text
bplus_tree_put(tree, key, data)
-> data != 0이면 bplus_tree_insert()
-> root부터 key 위치를 따라 내려감
-> leaf에 도착하면 leaf_insert()
```

`bplus_tree_insert()`는 non-leaf를 만날 때마다 `key_binary_search()` 결과로 child offset을 고른다.

```text
non-leaf에서 key보다 작은/큰 구간 판단
-> sub[] offset으로 다음 node를 읽음
-> leaf가 나올 때까지 반복
```

leaf에 도착하면 `leaf_insert()`가 처리한다.

```text
leaf에 같은 key가 이미 있음
-> -1 반환

leaf에 빈 공간이 있음
-> leaf_simple_insert()

leaf가 가득 참
-> leaf_split_left() 또는 leaf_split_right()
-> parent_node_build()
```

`leaf_simple_insert()`는 배열을 오른쪽으로 밀고 새 key/data를 정렬된 위치에 넣는다.

```text
기존 leaf:
[10, 30, 40]

20 삽입:
[10, 20, 30, 40]
```

이때 data도 key와 같은 위치로 같이 이동한다.

```text
key[]  이동
data[] 이동
```

## 6. leaf split: separator는 오른쪽 leaf의 첫 key다

`012`에서는 가득 찬 node에 새 key를 넣으면 node를 둘로 나누고 가운데 key를 parent로 올린다고 설명한다.

B+Tree leaf split에서는 약간 다르다.

- leaf에는 실제 key/data가 모두 남아 있어야 한다.
- parent로 올라가는 key는 data를 들고 올라가는 값이 아니라 separator다.
- 이 구현에서는 split 후 오른쪽 역할을 하는 leaf의 첫 key를 separator로 parent에 전달한다.

leaf가 가득 찬 경우 `leaf_insert()`는 삽입 위치에 따라 두 함수 중 하나를 호출한다.

```text
삽입 위치가 split 기준 왼쪽
-> leaf_split_left()

삽입 위치가 split 기준 오른쪽
-> leaf_split_right()
```

두 함수는 새 sibling leaf를 만들고, 기존 key/data를 둘로 나눈다.

```text
가득 찬 leaf + 새 key
-> left leaf
-> right leaf
-> parent에 올릴 separator key
```

그리고 leaf끼리 순서대로 훑을 수 있도록 linked list도 갱신한다.

```c
left_node_add(tree, leaf, left)
right_node_add(tree, leaf, right)
```

`left_node_add()`와 `right_node_add()`는 `prev`, `next`를 조정한다.

```text
기존 leaf linked list:
L <-> N <-> R

왼쪽 sibling 추가:
L <-> New <-> N <-> R

오른쪽 sibling 추가:
L <-> N <-> New <-> R
```

이 leaf linked list는 B+Tree가 range scan에 유리한 이유다.

## 7. parent 생성과 split 전파

`012`에서는 split된 가운데 key가 parent로 올라가고, parent도 가득 차 있으면 split이 재귀적으로 전파된다고 설명한다.

이 구현에서 그 중심 함수는 `parent_node_build()`다.

```text
parent가 없음
-> 새 non-leaf parent 생성
-> tree->root를 새 parent로 변경
-> tree->level 증가

parent가 있음
-> non_leaf_insert()로 separator를 parent에 삽입
```

처음 leaf root가 split되면 새 root가 생긴다.

```text
split 전:
root leaf

split 후:
        new root non-leaf
        /              \
 left leaf          right leaf
```

이미 parent가 있으면 `non_leaf_insert()`가 separator key와 두 child를 parent에 넣는다.

parent에 공간이 있으면 `non_leaf_simple_insert()`로 끝난다.

parent도 가득 차 있으면 non-leaf split이 발생한다.

```text
non_leaf_insert()
-> parent가 full인지 확인
-> full이면 split 위치에 따라
   non_leaf_split_left()
   non_leaf_split_middle()
   non_leaf_split_right()
-> split_key를 다시 parent_node_build()로 올림
```

이 흐름이 `012`의 "split은 재귀적으로 전파될 수 있다"는 설명의 실제 구현이다.

```text
leaf split
-> separator를 parent에 삽입
-> parent overflow
-> non-leaf split
-> split_key를 grandparent에 삽입
-> 필요하면 root까지 반복
```

## 8. non-leaf split은 child parent도 고쳐야 한다

non-leaf node는 leaf와 다르게 child node를 가리킨다. 그래서 non-leaf를 split하면 key와 child offset만 나누는 것으로 끝나지 않는다.

새 sibling으로 옮겨진 child들은 parent offset이 바뀌어야 한다.

이 작업을 하는 함수가 다음 두 개다.

```c
sub_node_update()
sub_node_flush()
```

역할은 다음과 같다.

- `sub_node_update()`: parent의 `sub[index]`를 child offset으로 설정하고, child의 `parent`를 새 parent offset으로 바꾼 뒤 flush한다.
- `sub_node_flush()`: 이미 `sub[]`에 들어 있는 child offset을 읽어서 child의 `parent`를 현재 parent로 바꾼 뒤 flush한다.

이 부분은 `012`의 추상 설명에는 잘 드러나지 않지만, 실제 파일 기반 tree에서는 중요하다.

```text
non-leaf split 후 child를 새 node로 옮김
-> child->parent가 예전 node를 가리키면 안 됨
-> 옮겨진 모든 child의 parent offset을 갱신해야 함
```

## 9. 삭제: public delete도 leaf에서 시작한다

`012`에서는 삭제할 key가 internal node에 있을 수도 있고, 이 경우 separator를 대체해야 한다고 설명한다.

이 B+Tree 구현에서는 외부에서 삭제하는 실제 data가 leaf에만 있다. 따라서 public delete는 항상 leaf까지 내려가서 leaf entry를 지운다.

```text
bplus_tree_put(tree, key, 0)
-> bplus_tree_delete()
-> root부터 leaf까지 내려감
-> leaf_remove()
```

`bplus_tree_delete()`도 검색과 같은 방식으로 child를 고른다.

```text
non-leaf에서 key와 같은 separator를 만나도
-> data가 아니므로 sub[i + 1]로 내려감
```

따라서 `012`의 "internal node의 key 삭제"는 이 구현에서 다음처럼 나타난다.

```text
사용자가 internal separator를 직접 삭제하는 것이 아님

leaf 삭제 또는 merge 결과로
parent의 separator가 더 이상 필요 없어지거나 값이 바뀜

그때 non_leaf_remove(), shift, merge 함수들이
internal separator를 조정함
```

## 10. leaf 삭제 보정: 빌리거나 merge한다

`012`에서는 삭제 후 node가 최소 key 수보다 작아지면 sibling에서 빌리거나 merge한다고 설명한다.

이 구현의 leaf 삭제 보정은 `leaf_remove()`에 들어 있다.

```text
leaf가 root임
-> 마지막 entry 삭제면 tree를 empty로 만듦
-> 아니면 leaf_simple_remove()

leaf가 root가 아님
-> 삭제 후 최소치 문제가 생길 수 있으면 sibling 확인
```

leaf의 최소 기준은 다음 조건으로 판단한다.

```c
leaf->children <= (_max_entries + 1) / 2
```

왼쪽 또는 오른쪽 sibling은 `sibling_select()`가 고른다.

```text
왼쪽 sibling이 없으면 오른쪽 선택
오른쪽 sibling이 없으면 왼쪽 선택
둘 다 있으면 children 수가 더 많은 sibling 선택
```

빌릴 수 있으면 shift를 한다.

```text
왼쪽 sibling에서 빌림
-> leaf_shift_from_left()
-> 왼쪽의 마지막 key/data를 현재 leaf 앞으로 이동
-> parent separator를 현재 leaf의 첫 key로 갱신

오른쪽 sibling에서 빌림
-> leaf_shift_from_right()
-> 오른쪽의 첫 key/data를 현재 leaf 뒤로 이동
-> parent separator를 오른쪽 leaf의 새 첫 key로 갱신
```

빌릴 수 없으면 merge한다.

```text
왼쪽 sibling과 merge
-> leaf_merge_into_left()
-> 현재 leaf 삭제
-> parent separator 제거를 위해 non_leaf_remove()

오른쪽 sibling과 merge
-> leaf_merge_from_right()
-> 오른쪽 leaf 삭제
-> parent separator 제거를 위해 non_leaf_remove()
```

여기서 `node_delete()`는 삭제된 node의 block을 free list에 넣고, leaf linked list의 `prev`, `next`도 이어 준다.

```text
L <-> Deleted <-> R
-> L <-> R
-> Deleted block은 free_blocks에 등록
```

## 11. non-leaf 삭제 보정도 같은 원리다

leaf merge가 일어나면 parent의 separator key 하나가 사라진다. 그러면 parent인 non-leaf도 최소 child 수를 못 지킬 수 있다.

이때 `non_leaf_remove()`가 재귀적으로 호출된다.

```text
leaf merge
-> parent separator 제거 필요
-> non_leaf_remove(parent, separator_index)
-> parent가 부족하면 parent의 sibling에서 빌리거나 merge
-> 더 위 parent로 전파될 수 있음
```

non-leaf 보정 함수들은 leaf 보정 함수와 이름이 거의 같다.

```text
non_leaf_shift_from_left()
non_leaf_shift_from_right()
non_leaf_merge_into_left()
non_leaf_merge_from_right()
```

차이는 non-leaf에는 data가 없고 child offset이 있다는 점이다.

```text
leaf 보정:
key[]와 data[]를 같이 이동

non-leaf 보정:
key[]와 sub[]를 같이 이동
child의 parent offset도 갱신
```

non-leaf에서 sibling에게 빌릴 때는 parent separator가 함께 회전한다.

```text
왼쪽 sibling에서 빌림:
parent separator를 현재 node로 내림
왼쪽 sibling의 마지막 key를 parent separator로 올림
왼쪽 sibling의 마지막 child를 현재 node로 옮김

오른쪽 sibling에서 빌림:
parent separator를 현재 node로 내림
오른쪽 sibling의 첫 key를 parent separator로 올림
오른쪽 sibling의 첫 child를 현재 node로 옮김
```

이것이 `012`의 "sibling key를 바로 가져오면 안 되고 parent separator도 조정해야 한다"는 설명의 실제 구현이다.

## 12. root는 예외다

`012`에서는 root node가 최소 key 수 규칙의 예외라고 설명한다.

이 구현도 root를 별도로 처리한다.

삽입에서는 empty tree에 첫 key를 넣을 때 leaf root를 만든다.

```text
tree->root == INVALID_OFFSET
-> leaf_new()
-> key/data 하나 저장
-> tree->root 설정
-> tree->level = 1
```

root leaf가 split되면 `parent_node_build()`가 새 non-leaf root를 만든다.

```text
root leaf split
-> 새 parent 생성
-> tree->root = parent->self
-> tree->level++
```

삭제에서는 root가 비면 tree를 줄인다.

```text
root leaf의 마지막 entry 삭제
-> tree->root = INVALID_OFFSET
-> tree->level = 0

root non-leaf의 child가 하나만 남음
-> 첫 child를 새 root로 승격
-> tree->level--
```

이 흐름은 B-Tree/B+Tree가 불필요한 root level을 유지하지 않도록 만든다.

## 13. range scan은 B+Tree의 leaf linked list를 사용한다

`012`의 마지막 부분에서는 B+Tree가 범위 검색에 유리하다고 설명한다.

이 구현에서는 leaf마다 `prev`, `next` offset이 있고, `bplus_tree_get_range()`가 이 연결을 따라간다.

흐름은 다음과 같다.

```text
min key가 들어갈 leaf까지 일반 검색처럼 내려감
-> leaf 안에서 시작 위치를 찾음
-> key <= max인 동안 data를 확인
-> leaf 끝에 도달하면 node->next로 다음 leaf를 읽음
```

다만 이 함수는 일반적인 iterator처럼 범위 안의 모든 값을 반환하지 않는다. 내부적으로 범위를 훑기는 하지만 반환값은 마지막으로 확인한 `data` 하나다.

```text
while range scan:
    start = data(node)[i]

return start
```

따라서 이 프로젝트에서 여러 row를 범위 조회하는 API로 쓰기에는 부족하고, 현재 문서화된 것처럼 외부 사용자는 단일 key 조회 중심으로 사용하는 편이 안전하다.

## 14. 미니 DB에서의 의미

이 프로젝트에서 B+Tree는 row 자체를 저장하는 공간이 아니다. row는 별도 table 파일에 있고, B+Tree leaf의 `data`는 row 위치를 가리키는 값으로 쓴다.

```text
key:
id column

data:
row가 table 파일 안에서 시작되는 byte offset
```

그래서 SQL 실행 흐름은 다음처럼 연결된다.

```text
SELECT ... WHERE id = ?
-> B+Tree에서 id 검색
-> data로 row byte offset 획득
-> table 파일에서 해당 위치로 이동
-> fixed-size row 읽기
```

주의할 점은 `bplus_tree_put()`의 `data == 0`이 삭제 명령이라는 것이다.

```c
int bplus_tree_put(struct bplus_tree *tree, key_t key, long data)
{
        if (data) {
                return bplus_tree_insert(tree, key, data);
        } else {
                return bplus_tree_delete(tree, key);
        }
}
```

따라서 실제 파일 offset `0`을 그대로 저장하면 삭제로 처리된다. 이 프로젝트의 wrapper는 이런 경우를 피하려면 `offset + 1`을 저장하고 읽을 때 `-1`로 복원해야 한다.

## 15. 전체 연결 요약

`012`의 개념과 `thirdparty/bplustree.c`의 구현은 다음처럼 대응된다.

| `012`의 개념 | 실제 구현 |
| --- | --- |
| node에 여러 key 저장 | `key(node)` 배열 |
| node가 여러 child로 분기 | non-leaf의 `sub(node)` offset 배열 |
| leaf에 도착할 때까지 검색 | `bplus_tree_search()`가 `node_seek()`로 child를 따라감 |
| node 접근 비용이 큼 | `pread()`/`pwrite()`로 block 단위 파일 I/O |
| leaf level에 삽입 | `bplus_tree_insert()`가 leaf까지 내려가 `leaf_insert()` 호출 |
| node가 가득 차면 split | `leaf_split_left/right()`, `non_leaf_split_left/middle/right()` |
| split key가 parent로 올라감 | `parent_node_build()`, `non_leaf_insert()` |
| split이 parent로 전파됨 | `non_leaf_insert()`가 다시 `parent_node_build()` 호출 |
| 삭제 후 sibling에서 빌림 | `leaf_shift_*()`, `non_leaf_shift_*()` |
| 삭제 후 sibling과 merge | `leaf_merge_*()`, `non_leaf_merge_*()` |
| merge가 parent로 전파됨 | `non_leaf_remove()` 재귀 호출 |
| root는 최소 key 수 예외 | root split, root shrink를 별도 처리 |
| B+Tree는 범위 검색에 유리 | leaf의 `prev`, `next`와 `bplus_tree_get_range()` |

정리하면 `thirdparty/bplustree.c`는 `012`에서 설명한 B-Tree의 큰 규칙을 파일 기반 B+Tree로 구체화한 코드다. 검색, 삽입, 삭제의 균형 유지 원리는 거의 그대로 유지되지만, 실제 data를 leaf에만 저장하고 internal node는 separator와 child offset만 가진다는 점이 가장 중요한 차이다.

# B-Tree 동작을 함수로 보는 추상 의사코드

이 문서는 `docs/012-btree-script-summary.md`의 B-Tree 설명을 아주 단순한 함수형 의사코드로 다시 정리한다.

실제 C 구현이 아니라 개념을 이해하기 위한 의사코드다. 따라서 포인터, 메모리 할당, 디스크 I/O, 에러 처리는 생략하고 B-Tree가 어떤 순서로 움직이는지만 보여준다.

## 1. 전제 모델

`012`의 핵심은 다음 네 가지다.

- node 안에는 여러 key가 정렬된 상태로 들어 있다.
- key 사이의 구간마다 child가 있다.
- 모든 leaf node는 같은 깊이에 있다.
- node가 너무 커지면 split하고, 너무 작아지면 borrow 또는 merge한다.

이 문서에서는 다음과 같은 추상 구조를 사용한다.

```text
MAX_KEYS = node 하나가 가질 수 있는 최대 key 수
MIN_KEYS = root가 아닌 node가 가져야 하는 최소 key 수

Tree:
    root

Node:
    keys
    children
    parent
    is_leaf
```

node의 key와 child 관계는 다음처럼 생각한다.

```text
keys = [10, 20, 30]

children[0] -> 10보다 작은 key들
children[1] -> 10과 20 사이 key들
children[2] -> 20과 30 사이 key들
children[3] -> 30보다 큰 key들
```

root는 예외다. root는 key가 적어도 허용된다.

## 2. 검색

`012`의 "root에서 시작해 key를 비교하고, 해당 child로 내려간다"는 내용을 함수로 쓰면 다음과 같다.

```text
def search(tree, target_key)
    return search_node(tree.root, target_key)
```

```text
def search_node(node, target_key)
    index = find_key_index(node, target_key)

    if index found
        return found node.keys[index]

    if node.is_leaf
        return not_found

    child_index = find_child_index(node, target_key)
    child = node.children[child_index]

    return search_node(child, target_key)
```

이 함수가 하는 일:

- 현재 node 안에서 key를 찾는다.
- 찾으면 바로 성공한다.
- 못 찾았고 leaf라면 실패한다.
- leaf가 아니라면 key가 들어갈 수 있는 child로 내려간다.

child를 고르는 함수는 아주 단순하다.

```text
def find_child_index(node, target_key)
    index = 0

    while index < count(node.keys)
        if target_key < node.keys[index]
            return index

        index = index + 1

    return count(node.keys)
```

이 함수가 하는 일:

- key 배열을 왼쪽부터 확인한다.
- target key보다 큰 key를 처음 만나면 그 왼쪽 child를 선택한다.
- 모든 key보다 target key가 크면 가장 오른쪽 child를 선택한다.

## 3. 삽입

`012`의 "삽입은 leaf level에서 일어난다"는 내용을 함수로 쓰면 다음과 같다.

```text
def insert(tree, new_key)
    if tree.root is empty
        tree.root = new_leaf_node(new_key)
        return

    leaf = find_leaf_for_insert(tree.root, new_key)
    insert_key_in_order(leaf, new_key)

    if count(leaf.keys) > MAX_KEYS
        split_if_needed(tree, leaf)
```

이 함수가 하는 일:

- tree가 비어 있으면 root leaf를 만든다.
- 비어 있지 않으면 새 key가 들어갈 leaf를 찾는다.
- leaf 안의 정렬된 위치에 key를 넣는다.
- leaf가 너무 커지면 split한다.

삽입할 leaf를 찾는 함수는 검색과 거의 같다.

```text
def find_leaf_for_insert(node, new_key)
    if node.is_leaf
        return node

    child_index = find_child_index(node, new_key)
    child = node.children[child_index]

    return find_leaf_for_insert(child, new_key)
```

정렬된 위치에 key를 넣는 함수는 다음처럼 생각한다.

```text
def insert_key_in_order(node, new_key)
    position = find_insert_position(node.keys, new_key)
    insert new_key into node.keys at position
```

예를 들어 `[10, 20, 40]`에 `30`을 넣으면 `[10, 20, 30, 40]`이 된다.

## 4. split

`012`의 "node가 가득 차면 둘로 나누고 가운데 key를 parent로 올린다"는 내용을 함수로 쓰면 다음과 같다.

```text
def split_if_needed(tree, node)
    if count(node.keys) <= MAX_KEYS
        return

    left, middle_key, right = split_node(node)
    insert_into_parent(tree, node, left, middle_key, right)
```

이 함수가 하는 일:

- node가 최대 key 수를 넘었는지 확인한다.
- 넘지 않았으면 아무것도 하지 않는다.
- 넘었다면 node를 left, middle key, right로 나눈다.
- middle key를 parent에 올린다.

node를 나누는 함수는 다음처럼 볼 수 있다.

```text
def split_node(node)
    middle_index = count(node.keys) / 2

    middle_key = node.keys[middle_index]

    left = new node
    right = new node

    left.keys = keys before middle_key
    right.keys = keys after middle_key

    if node is not leaf
        left.children = children before middle split
        right.children = children after middle split

    return left, middle_key, right
```

이 함수가 하는 일:

- 가운데 key를 separator로 고른다.
- 가운데 key보다 작은 key들은 left로 보낸다.
- 가운데 key보다 큰 key들은 right로 보낸다.
- internal node라면 child들도 key 구간에 맞게 나눈다.

## 5. parent에 split 결과 넣기

split 후에는 가운데 key를 parent에 넣어야 한다. `012`의 "split이 parent로 전파될 수 있다"는 설명이 여기서 나온다.

```text
def insert_into_parent(tree, old_node, left, middle_key, right)
    parent = old_node.parent

    if parent is empty
        new_root = new internal node
        new_root.keys = [middle_key]
        new_root.children = [left, right]

        left.parent = new_root
        right.parent = new_root
        tree.root = new_root
        return

    replace old_node in parent.children with left and right
    insert middle_key into parent.keys in order

    left.parent = parent
    right.parent = parent

    if count(parent.keys) > MAX_KEYS
        split_if_needed(tree, parent)
```

이 함수가 하는 일:

- split된 node가 root였다면 새 root를 만든다.
- parent가 있다면 parent 안에 separator key와 새 child를 넣는다.
- parent도 너무 커지면 parent를 다시 split한다.

즉 split 전파는 다음처럼 반복된다.

```text
leaf split
-> parent에 middle key 삽입
-> parent overflow
-> parent split
-> grandparent에 middle key 삽입
-> 필요하면 root까지 반복
```

## 6. 삭제

`012`의 "삭제도 먼저 검색으로 key를 찾는다"는 내용을 함수로 쓰면 다음과 같다.

```text
def delete(tree, target_key)
    node, index = find_node_containing_key(tree.root, target_key)

    if node not found
        return not_found

    delete_from_node(tree, node, index)
    return success
```

이 함수가 하는 일:

- key가 들어 있는 node를 찾는다.
- 없으면 실패한다.
- 있으면 해당 node에서 삭제를 시작한다.

실제 삭제 함수는 leaf인지 internal node인지에 따라 나뉜다.

```text
def delete_from_node(tree, node, key_index)
    if node.is_leaf
        remove key at key_index from node.keys
        rebalance_after_delete(tree, node)
        return

    replacement_key, replacement_leaf = find_replacement_key(node, key_index)

    node.keys[key_index] = replacement_key
    remove replacement_key from replacement_leaf.keys

    rebalance_after_delete(tree, replacement_leaf)
```

이 함수가 하는 일:

- leaf key라면 바로 지운다.
- internal key라면 그 key가 subtree를 나누는 separator라서 바로 지우지 않는다.
- 대신 대체 key를 찾아 internal node에 넣고, 대체 key가 있던 leaf에서 삭제한다.

## 7. internal key 삭제

`012`에서는 internal node의 key를 삭제할 때 왼쪽 subtree의 최댓값 또는 오른쪽 subtree의 최솟값으로 대체한다고 설명한다.

```text
def find_replacement_key(node, key_index)
    left_child = node.children[key_index]
    right_child = node.children[key_index + 1]

    if left_child has enough keys
        leaf = find_rightmost_leaf(left_child)
        key = last key in leaf
        return key, leaf

    leaf = find_leftmost_leaf(right_child)
    key = first key in leaf
    return key, leaf
```

이 함수가 하는 일:

- 삭제하려는 key의 왼쪽 subtree와 오른쪽 subtree를 본다.
- 왼쪽 subtree에서 가장 큰 key를 가져오거나, 오른쪽 subtree에서 가장 작은 key를 가져온다.
- 가져온 key가 internal node의 새 separator가 된다.

오른쪽 끝 leaf와 왼쪽 끝 leaf를 찾는 함수는 단순하다.

```text
def find_rightmost_leaf(node)
    while node is not leaf
        node = last child of node

    return node
```

```text
def find_leftmost_leaf(node)
    while node is not leaf
        node = first child of node

    return node
```

## 8. 삭제 후 보정

`012`의 "삭제 후 node가 너무 작아지면 sibling에서 빌리거나 merge한다"는 내용을 함수로 쓰면 다음과 같다.

```text
def rebalance_after_delete(tree, node)
    if node is tree.root
        shrink_root_if_needed(tree)
        return

    if count(node.keys) >= MIN_KEYS
        return

    left_sibling = get_left_sibling(node)
    right_sibling = get_right_sibling(node)

    if left_sibling exists and count(left_sibling.keys) > MIN_KEYS
        borrow_from_left(node, left_sibling)
        return

    if right_sibling exists and count(right_sibling.keys) > MIN_KEYS
        borrow_from_right(node, right_sibling)
        return

    if left_sibling exists
        merge_with_left(tree, node, left_sibling)
        return

    merge_with_right(tree, node, right_sibling)
```

이 함수가 하는 일:

- root라면 root 예외 규칙으로 처리한다.
- 최소 key 수를 지키고 있으면 끝난다.
- sibling이 여유 key를 가지고 있으면 하나 빌린다.
- 빌릴 수 없으면 sibling과 합친다.

## 9. 왼쪽 sibling에서 빌리기

`012`에서는 sibling에서 key를 바로 가져오면 안 되고 parent separator도 함께 조정해야 한다고 설명한다.

```text
def borrow_from_left(node, left_sibling)
    parent = node.parent
    separator_index = index between left_sibling and node in parent

    borrowed_key = last key of left_sibling
    separator_key = parent.keys[separator_index]

    remove borrowed_key from left_sibling.keys

    insert separator_key at front of node.keys
    parent.keys[separator_index] = borrowed_key

    if node is not leaf
        moved_child = last child of left_sibling
        move moved_child to front of node.children
```

이 함수가 하는 일:

- parent의 separator key를 부족한 node로 내린다.
- 왼쪽 sibling의 가장 큰 key를 parent의 새 separator로 올린다.
- internal node라면 child도 함께 옮긴다.

## 10. 오른쪽 sibling에서 빌리기

오른쪽 sibling에서 빌릴 때도 parent separator를 함께 조정한다.

```text
def borrow_from_right(node, right_sibling)
    parent = node.parent
    separator_index = index between node and right_sibling in parent

    borrowed_key = first key of right_sibling
    separator_key = parent.keys[separator_index]

    remove borrowed_key from right_sibling.keys

    append separator_key to node.keys
    parent.keys[separator_index] = borrowed_key

    if node is not leaf
        moved_child = first child of right_sibling
        move moved_child to end of node.children
```

이 함수가 하는 일:

- parent의 separator key를 부족한 node로 내린다.
- 오른쪽 sibling의 가장 작은 key를 parent의 새 separator로 올린다.
- internal node라면 child도 함께 옮긴다.

## 11. 왼쪽 sibling과 merge하기

빌릴 수 있는 sibling이 없다면 merge한다.

```text
def merge_with_left(tree, node, left_sibling)
    parent = node.parent
    separator_index = index between left_sibling and node in parent
    separator_key = parent.keys[separator_index]

    append separator_key to left_sibling.keys
    append all node.keys to left_sibling.keys

    if node is not leaf
        append all node.children to left_sibling.children

    remove separator_key from parent.keys
    remove node from parent.children

    rebalance_after_delete(tree, parent)
```

이 함수가 하는 일:

- 왼쪽 sibling, parent separator, 현재 node를 하나로 합친다.
- parent에서는 separator key와 현재 node child가 사라진다.
- parent도 너무 작아질 수 있으므로 삭제 보정을 parent에 다시 적용한다.

## 12. 오른쪽 sibling과 merge하기

오른쪽 sibling과 merge할 때도 원리는 같다.

```text
def merge_with_right(tree, node, right_sibling)
    parent = node.parent
    separator_index = index between node and right_sibling in parent
    separator_key = parent.keys[separator_index]

    append separator_key to node.keys
    append all right_sibling.keys to node.keys

    if node is not leaf
        append all right_sibling.children to node.children

    remove separator_key from parent.keys
    remove right_sibling from parent.children

    rebalance_after_delete(tree, parent)
```

이 함수가 하는 일:

- 현재 node, parent separator, 오른쪽 sibling을 하나로 합친다.
- parent에서는 separator key와 오른쪽 sibling child가 사라진다.
- parent가 작아졌다면 다시 borrow 또는 merge를 시도한다.

## 13. root 예외 처리

`012`에서는 root가 최소 key 수 규칙의 예외라고 설명한다.

```text
def shrink_root_if_needed(tree)
    root = tree.root

    if root is leaf
        if count(root.keys) == 0
            tree.root = empty
        return

    if count(root.keys) == 0
        only_child = root.children[0]
        only_child.parent = empty
        tree.root = only_child
```

이 함수가 하는 일:

- root leaf가 비면 tree 전체가 empty가 된다.
- internal root의 key가 없어지고 child가 하나만 남으면 그 child를 새 root로 올린다.
- 이렇게 해서 불필요하게 한 층 높은 tree를 유지하지 않는다.

## 14. 전체 흐름 요약

검색은 가장 단순하다.

```text
search
-> 현재 node에서 key 비교
-> 찾으면 성공
-> 못 찾으면 child 선택
-> leaf까지 반복
```

삽입은 leaf에서 시작해 위로 전파될 수 있다.

```text
insert
-> 들어갈 leaf 찾기
-> leaf에 정렬 삽입
-> leaf overflow면 split
-> parent overflow면 split 반복
```

삭제는 leaf에서 끝나거나, internal key를 leaf key로 대체한 뒤 보정한다.

```text
delete
-> key 찾기
-> leaf key면 삭제
-> internal key면 대체 key로 바꾼 뒤 leaf에서 삭제
-> node underflow면 sibling에서 borrow
-> borrow 불가하면 merge
-> parent underflow면 위로 반복
```

결국 B-Tree의 동작은 세 가지 규칙을 계속 지키는 과정이다.

- key는 항상 정렬된 상태로 둔다.
- 모든 leaf는 같은 깊이에 둔다.
- node 크기는 root 예외를 제외하고 최소와 최대 범위 안에 둔다.

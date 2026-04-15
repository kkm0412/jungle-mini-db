# B-Tree 핵심 개념 정리

> [Understanding B-Trees: The Data Structure Behind Modern Databases](https://youtu.be/K1a2Bk8NrYQ?si=KGgK1ygdtvOnHrtd) 자료의 내용을 기반으로 한다.

이 문서는 B-Tree가 왜 데이터베이스와 파일 시스템에서 자주 사용되는지, 그리고 검색, 삽입, 삭제가 어떤 규칙으로 동작하는지 정리한다.

핵심은 단순하다.

- 데이터를 정렬된 상태로 유지한다.
- 한 노드에 여러 key를 저장해 트리 높이를 낮춘다.
- 모든 leaf node를 같은 깊이에 두어 균형을 유지한다.
- 노드가 너무 커지면 나누고, 너무 작아지면 빌리거나 합친다.

## 1. 이진 탐색 트리에서 출발하기

이진 탐색 트리는 각 node가 하나의 key를 가지고, 왼쪽 child와 오른쪽 child를 가질 수 있는 자료구조다.

이진 탐색 트리의 규칙은 다음과 같다.

- 어떤 node의 왼쪽 subtree에는 그 node의 key보다 작은 값만 있다.
- 어떤 node의 오른쪽 subtree에는 그 node의 key보다 큰 값만 있다.

이 규칙 덕분에 검색할 때 모든 node를 확인하지 않아도 된다. 찾는 key가 현재 node보다 작으면 왼쪽만 보면 되고, 크면 오른쪽만 보면 된다.

```text
현재 node 확인
-> 찾는 key가 더 작음
-> 왼쪽 subtree만 검색

현재 node 확인
-> 찾는 key가 더 큼
-> 오른쪽 subtree만 검색
```

즉, 매 단계마다 검색할 필요가 없는 절반 정도의 공간을 버릴 수 있다.

## 2. 더 많이 분기하면 항상 더 빠를까?

이진 탐색 트리는 한 node에서 두 방향으로 나뉜다. 그렇다면 한 node에서 세 방향 이상으로 나뉘는 트리를 만들면 더 빠를 것처럼 보인다.

예를 들어 한 node가 key 두 개를 가진다면 세 구간으로 나눌 수 있다.

```text
[10 | 20]

10보다 작은 값       -> 왼쪽 child
10과 20 사이의 값    -> 가운데 child
20보다 큰 값         -> 오른쪽 child
```

이렇게 하면 트리 높이는 낮아진다. 검색할 때 거쳐야 하는 node 수가 줄어들 수 있기 때문이다.

하지만 항상 더 효율적인 것은 아니다. 한 node 안에 key가 여러 개 있으면, 그 node 안에서 비교해야 하는 횟수도 늘어난다. 이진 탐색 트리에서는 한 node에서 key 하나만 비교하면 되지만, key가 두 개라면 경우에 따라 두 번 비교해야 한다.

따라서 어떤 방식이 더 빠른지는 무엇이 비싼 작업인지에 따라 달라진다.

## 3. B-Tree가 유리한 환경

CPU가 key를 비교하는 작업은 매우 빠르다. 반면 데이터베이스나 파일 시스템처럼 많은 데이터를 다루는 환경에서는 새 node를 가져오는 작업이 훨씬 비싸다.

특히 데이터가 메모리에 모두 올라와 있지 않고 디스크나 파일에서 읽어야 한다면, 비교 연산보다 데이터 접근 시간이 더 큰 병목이 된다.

B-Tree는 이 상황에서 강하다.

- 한 node에 여러 key를 저장한다.
- 한 node가 여러 child로 분기할 수 있다.
- 트리 높이가 낮아진다.
- 검색 중 접근해야 하는 node 수가 줄어든다.

즉, B-Tree는 비교 횟수를 조금 더 쓰더라도 비싼 node 접근 횟수를 줄이는 쪽을 선택한 구조다.

## 4. B-Tree node 구조와 검색

B-Tree의 node는 여러 key를 가질 수 있다. 예를 들어 한 node가 최대 4개의 key를 가진다고 가정할 수 있다.

node에 key가 `n`개 있으면 child는 최대 `n + 1`개가 된다. key들이 구간을 나누고, 각 child는 해당 구간에 속한 key들을 담당한다.

```text
[10 | 20 | 30]

10보다 작은 값        -> child 0
10과 20 사이의 값     -> child 1
20과 30 사이의 값     -> child 2
30보다 큰 값          -> child 3
```

검색 과정은 다음과 같다.

1. root node에서 시작한다.
2. node 안의 key들과 찾는 key를 비교한다.
3. 같은 key가 있으면 검색을 끝낸다.
4. 없으면 key가 속할 구간의 child로 내려간다.
5. 찾거나, 더 내려갈 child가 없을 때까지 반복한다.

한 node가 여러 방향으로 분기하므로, B-Tree는 이진 탐색 트리보다 훨씬 적은 node 접근으로 원하는 key를 찾을 수 있다.

## 5. B-Tree의 균형 규칙

B-Tree는 검색 성능을 안정적으로 유지하기 위해 몇 가지 규칙을 가진다.

### 5.1 모든 leaf node는 같은 level에 있다

leaf node는 더 이상 child를 가리키지 않는 node다. B-Tree에서는 모든 leaf node가 같은 깊이에 있어야 한다.

이 규칙 덕분에 트리가 한쪽으로만 깊어지는 상황을 막을 수 있다.

```text
허용됨:
root
-> internal
-> leaf

허용되지 않음:
한쪽 leaf는 depth 2
다른쪽 leaf는 depth 4
```

### 5.2 node에는 최대 key 수와 최소 key 수가 있다

B-Tree는 node가 가질 수 있는 최대 key 수를 정한다. 그리고 root가 아닌 node는 최소 key 수도 지켜야 한다.

예를 들어 최대 key 수가 4라면 최소 key 수는 보통 그 절반인 2로 볼 수 있다.

```text
최대 key 수: 4
최소 key 수: 2

일반 node: 2개, 3개, 4개의 key를 가질 수 있음
root node: 예외적으로 더 적은 key를 가질 수 있음
```

root node는 예외다. 처음 tree를 만들 때는 key 하나만 가진 root부터 시작할 수 있다.

## 6. 삽입 과정

B-Tree에 새 key를 넣을 때는 항상 leaf level에 삽입한다.

기본 흐름은 다음과 같다.

```text
root에서 시작
-> key가 들어갈 child를 따라 내려감
-> leaf node에 도착
-> 정렬된 위치에 key 삽입
```

대부분의 삽입은 단순하다. leaf node에 빈 공간이 있으면 그 위치에 key를 넣으면 된다.

문제는 node가 이미 가득 찬 상태에서 새 key를 넣어야 할 때 생긴다. 이 경우 node를 split한다.

## 7. node split

최대 key 수가 4인 B-Tree에 다섯 번째 key를 넣는다고 생각해보자. 하나의 node에는 key를 4개까지만 저장할 수 있으므로 그대로 둘 수 없다.

이때 B-Tree는 node를 둘로 나누고, 가운데 key를 parent로 올린다.

```text
삽입 후 임시 상태:
[10 | 20 | 30 | 40 | 50]

split 결과:
left node   = [10 | 20]
middle key  = 30
right node  = [40 | 50]

30은 parent로 올라감
```

가운데 key는 두 node 사이를 구분하는 separator 역할을 한다.

- left node의 모든 key는 middle key보다 작다.
- right node의 모든 key는 middle key보다 크다.

만약 split한 node가 root였다면, 올라간 middle key로 새 root를 만든다.

```text
        [30]
       /    \
[10 | 20]  [40 | 50]
```

트리 높이가 증가하는 경우는 root가 split될 때뿐이다.

## 8. split은 재귀적으로 전파될 수 있다

leaf node가 가득 차서 split되면 middle key가 parent로 올라간다. 그런데 parent도 이미 가득 차 있었다면 parent 역시 overflow 상태가 된다.

이 경우 같은 split 과정을 parent에도 적용한다.

```text
leaf split
-> middle key를 parent에 삽입
-> parent가 overflow
-> parent split
-> middle key를 그 위 parent에 삽입
-> 필요하면 root까지 반복
```

이 재귀적인 split 덕분에 B-Tree는 삽입 후에도 다음 규칙을 유지한다.

- 각 node는 최대 key 수를 넘지 않는다.
- root가 아닌 node는 최소 key 수를 지킨다.
- 모든 leaf node는 같은 level에 있다.
- 트리는 한쪽으로 치우치지 않는다.

## 9. 삭제 과정

B-Tree에서 key를 삭제할 때도 먼저 일반 검색과 같은 방식으로 key를 찾는다.

key를 찾은 뒤에는 그 key를 제거한다. 다만 삭제 후 node의 key 수가 최소 key 수보다 적어질 수 있다. 이 경우 B-Tree 규칙이 깨지므로 추가 처리가 필요하다.

삭제에서 문제가 되는 경우는 크게 두 가지다.

- leaf node에서 삭제했더니 최소 key 수보다 적어진 경우
- internal node의 key를 삭제해야 하는 경우

## 10. sibling에서 key 빌리기

삭제 후 어떤 node의 key 수가 최소보다 적어졌다면, 먼저 인접한 sibling node에서 key를 빌릴 수 있는지 확인한다.

오른쪽 sibling에서 빌린다면 가장 작은 key를 가져올 수 있고, 왼쪽 sibling에서 빌린다면 가장 큰 key를 가져올 수 있다.

하지만 sibling의 key를 바로 가져오면 안 된다. parent에 있는 separator key도 함께 조정해야 한다.

예를 들어 오른쪽 sibling에서 key를 빌리는 경우 흐름은 다음과 같다.

```text
1. parent의 separator key를 부족한 node로 내린다.
2. 오른쪽 sibling의 가장 작은 key를 parent의 새 separator로 올린다.
```

이렇게 해야 separator 규칙이 유지된다.

- separator보다 작은 값은 왼쪽에 있다.
- separator보다 큰 값은 오른쪽에 있다.

## 11. sibling과 merge하기

sibling도 이미 최소 key 수만 가지고 있다면 key를 빌릴 수 없다. 이 경우 node를 합친다.

merge는 다음 세 요소를 하나의 node로 합치는 과정이다.

- key가 부족한 node
- sibling node
- 두 node 사이에 있던 parent의 separator key

```text
left node + separator + right node
-> 하나의 node로 merge
```

merge가 일어나면 parent에서는 separator key가 사라진다. 이때 parent의 key 수가 최소보다 적어질 수 있다.

그 경우 같은 삭제 보정 과정을 parent에 재귀적으로 적용한다.

```text
node merge
-> parent key 수 감소
-> parent가 최소 key 수보다 작아짐
-> parent도 sibling에서 빌리거나 merge
```

## 12. internal node의 key 삭제

삭제하려는 key가 leaf가 아니라 internal node에 있을 수도 있다. 이 key는 두 subtree를 나누는 separator 역할을 하고 있으므로 그냥 지우면 안 된다.

이 경우 separator를 대체할 key가 필요하다. 가능한 선택지는 두 가지다.

- 왼쪽 subtree에서 가장 큰 key
- 오른쪽 subtree에서 가장 작은 key

둘 중 하나를 internal node의 새 separator로 올린다.

```text
삭제할 key = K

대체 가능:
왼쪽 subtree의 최댓값
또는
오른쪽 subtree의 최솟값
```

대체 key를 가져온 뒤에는, 그 key가 원래 있던 node에서 삭제가 발생한 것과 같다. 만약 그 node의 key 수가 최소보다 적어지면 앞에서 설명한 방식대로 sibling에서 빌리거나 merge한다.

## 13. B-Tree가 데이터 저장에 적합한 이유

B-Tree는 많은 데이터를 다룰 때 특히 유리하다.

데이터베이스나 파일 시스템에서는 전체 데이터를 처음부터 끝까지 훑는 것보다, 필요한 위치로 빠르게 좁혀 들어가는 것이 중요하다. B-Tree는 한 node에서 많은 구간으로 분기하기 때문에 적은 node 접근으로 검색 범위를 크게 줄일 수 있다.

또한 node가 항상 꽉 차 있을 필요는 없다. node는 최소 절반 정도만 채워져 있어도 된다. 이 여유 공간 덕분에 새 key를 삽입할 때 대부분은 단순히 빈 공간에 넣으면 되고, 매번 큰 구조 변경을 하지 않아도 된다.

정리하면 B-Tree의 장점은 다음과 같다.

- 트리 높이가 낮아 node 접근 횟수가 적다.
- 모든 leaf가 같은 level에 있어 검색 성능이 안정적이다.
- node가 너무 커지면 split으로 나눈다.
- node가 너무 작아지면 sibling에서 빌리거나 merge한다.
- 삽입과 삭제 후에도 균형 상태를 유지한다.

이런 특성 때문에 B-Tree와 그 변형은 데이터베이스 인덱스와 파일 시스템에서 자주 사용된다.

## 14. B-Tree와 B+Tree 연결

B+Tree는 B-Tree 계열의 변형이다. 둘은 여러 key를 가진 균형 tree라는 큰 구조를 공유한다.

차이는 주로 데이터가 어디에 저장되는지에 있다. B+Tree에서는 실제 데이터나 데이터 위치를 leaf node 쪽에 모으고, internal node는 검색 경로를 안내하는 separator 역할에 더 집중한다.

데이터베이스 인덱스에서는 이 구조가 특히 유용하다.

- 특정 key 검색을 빠르게 할 수 있다.
- leaf node를 따라가며 범위 검색을 처리하기 좋다.
- 테이블 전체를 읽지 않고 필요한 row 위치를 찾을 수 있다.

따라서 B-Tree를 이해하면, 데이터베이스 인덱스에서 자주 등장하는 B+Tree도 훨씬 자연스럽게 이해할 수 있다.

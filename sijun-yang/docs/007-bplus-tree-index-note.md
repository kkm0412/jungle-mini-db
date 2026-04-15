# WEEK7 수요코딩회 관련 핵심 개념 정리

## B+ Tree

설명하는 자료를 보면, B Tree와 B+ Tree를 혼용해서 사용하기도 함.

정확히는 약간 차이가 있지만, 개념적으로는 상당수 같은 구조를 공유하므로 B Tree를 먼저 찾아보는게 나음.

[What are the differences between B trees and B+ trees?](https://stackoverflow.com/questions/870218/what-are-the-differences-between-b-trees-and-b-trees)

### B Tree 핵심 개념

B Tree는 하나의 노드가 여러 key와 여러 child를 가질 수 있는 균형 트리다. 

key가 정렬된 상태로 저장되기 때문에 검색할 때 불필요한 범위를 빠르게 제외할 수 있다.

데이터가 추가되어도 트리 높이가 급격히 커지지 않도록 균형을 유지하는 것이 핵심이다.

#### 왜 효율적인가?

AVL Tree도 균형을 유지하지만, 한 노드가 최대 2개의 child만 가지는 이진 트리다.

반면 B Tree는 한 노드가 여러 key와 child를 가질 수 있어서 같은 개수의 데이터를 더 낮은 높이로 관리할 수 있다.

특히 파일이나 디스크처럼 한 번 접근하는 비용이 큰 환경에서는 트리 높이가 낮을수록 접근 횟수를 줄일 수 있어 유리하다.
(약 100,000배 차이가 날수도 있음 - [Latency Numbers Every Programmer Should Know](https://gist.github.com/jboner/2841832))

#### B+ Tree

특별한 형태의 B Tree로 봐도 무방하다.

B+ Tree는 실제 데이터 위치를 leaf node 쪽에 모아두는 방식으로 이해하면 된다. 

internal node는 검색 경로를 안내하는 역할에 가깝고, leaf node에서 최종 결과를 찾는다.    
(node의 종류는 Root, Internal nodes, Leaf nodes가 있다)

DB 인덱스에서는 이 구조 덕분에 특정 key 검색과 범위 검색을 모두 효율적으로 처리할 수 있다.

### 추천 자료
#### B Tree 
(B+ Tree의 기반이 됨)

1. 추천: [Understanding B-Trees: The Data Structure Behind Modern Databases](https://youtu.be/K1a2Bk8NrYQ?si=6O5mZFPAPsXe54U3)
2. 쉬운코드 영상
   1. [BJ.54-1 B tree의 개념과 특징, 데이터 삽입이 어떻게 동작하는지를 설명합니다! (DB 인덱스과 관련있는 자료 구조)](https://youtu.be/bqkcoSm_rCs?si=KVDQBdxIhbjcWZyF)
   2. [BJ.54-2 B tree 데이터 삭제 동작 방식을 설명합니다 (DB 인덱스과 관련있는 자료 구조)](https://youtu.be/H_u28u0usjA?si=0cl5zs8dRz8L8JGe)
   3. [BJ.54-3 B tree가 왜 DB 인덱스(index)로 사용되는지를 설명합니다](https://youtu.be/liPSnc6Wzfk?si=RDVl6kQZ1SgZZnPe)

#### B+ Tree
순차적 IO, 랜덤 IO
1. [B+ tree](https://en.wikipedia.org/wiki/B%2B_tree)
2. [Data Structures powering our Database Part-3 | B-Trees](https://www.linkedin.com/pulse/data-structures-powering-our-database-part-3-b-trees-saurav-prateek/)

## 파일 접근

이러한 B Tree 기반의 자료구조 효율적이라는 것 까지는 알았지만, 그래서 디스크(2차 저장장치)에 저장되어있는 데이터에 어떻게 빠르게 접근할 수 있을까?

인덱스는 key를 이용해 파일 안의 레코드 위치를 찾기 위한 구조다.

원하는 레코드의 위치를 알면 파일 전체를 처음부터 끝까지 읽지 않고 해당 위치로 바로 접근할 수 있다. 

따라서 파일 기반 DB에서는 B+ Tree가 레코드 자체보다 레코드의 파일 내 위치를 찾도록 연결되는 것이 중요하다.

### 순차적 I/O(Sequential I/O)와 랜덤 I/O(Random I/O)

순차적 I/O는 파일을 앞에서부터 이어서 읽는 방식이고, 랜덤 I/O는 필요한 위치로 바로 이동해서 읽는 방식이다.

순차적 I/O는 연속된 데이터를 읽기 때문에 접근 흐름이 단순하고 예측하기 쉽다.

랜덤 I/O는 여러 위치를 건너뛰며 접근하므로 저장장치가 다음 위치를 찾는 비용이 추가될 수 있다. HDD 같은 저장장치에서는 이 비용이 물리적 이동으로 이어져 더 크게 나타날 수 있다.

[HDD 돌아가는거 보기](https://youtu.be/ojGvHDjHPb4?si=vivrgFbO0Fdh5yFn)

### 읽는 데이터 양과 선택도(Selectivity)

테이블 풀 스캔은 파일을 앞에서부터 읽는 순차적 I/O에 가깝지만, 조건에 맞는 레코드를 찾기 위해 전체 데이터를 확인해야 한다.

인덱스 검색은 필요한 위치로 이동하는 과정에서 랜덤 I/O에 가까운 접근이 생길 수 있다.

그럼에도 효율적인 이유는 전체 테이블을 읽는 대신 인덱스와 필요한 레코드만 읽기 때문에, 읽는 데이터 양 자체가 크게 줄어들기 때문이다.

따라서 인덱스로 너무 많은 레코드를 읽어야 하는 경우에는 테이블 풀 스캔이 더 효율적일 수 있다. (비용 역전 지점 - 공식 용어는 아님)

이처럼 조건이 전체 데이터 중 얼마나 적은 비율을 골라내는지를 선택도(selectivity)라고 하며, 선택도가 낮을수록 인덱스가 유리해진다.

[그냥 이해 돕는 이미지 바로가기용 링크 - 참고자료 아님](https://j-d-i.tistory.com/m/360)

### 인덱스 구조의 트레이드오프

인덱스는 테이블 파일과 별개의 구조를 추가로 관리해야 하므로, INSERT 같은 쓰기 작업에서는 인덱스도 함께 갱신해야 하는 오버헤드가 생긴다.

대신 SELECT에서는 전체 테이블을 읽지 않고 필요한 레코드 위치를 빠르게 찾을 수 있어 읽기 성능이 좋아진다.

즉, 인덱스는 쓰기와 파일 관리 비용을 더 부담하는 대신 읽기 속도를 높이는 구조다.

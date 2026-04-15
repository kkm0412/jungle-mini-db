# 업데이트 내용
1. 테이블 포맷팅 
=> 출력 할때 테이블 보기 좋게 출력되게 
<img width="281" height="127" alt="image" src="https://github.com/user-attachments/assets/cb07b4f9-2b37-4d7b-92a7-c586bbd2619c" />

2. SQL 쿼리 case insensitive
<img width="289" height="258" alt="image" src="https://github.com/user-attachments/assets/92b356ad-4e46-4071-a4e7-986b3b8fe032" />

3. BETWEEN 절 작동하도록 구현 

<img width="1412" height="783" alt="image" src="https://github.com/user-attachments/assets/5f670ac2-de35-453a-8ccb-1c01f0f22267" />

# B+Tree BETWEEN 성능 비교

테스트 조건:

- 데이터 수: 1,000,000 rows
- Row 크기: 64 bytes fixed row
- 쿼리: `SELECT * FROM users WHERE id BETWEEN 999991 AND 1000000;`
- 결과 row 수: 10 rows
- 반복 횟수: 50회
- 빌드 옵션: `clang -O2`

| 방식 | 총 실행 시간 | 평균 실행 시간 | 비고 |
|---|---:|---:|---|
| B+Tree 인덱스 BETWEEN | 0.0113초 | 0.226ms | 인덱스로 시작 id 위치를 찾고 leaf를 순회 |
| 선형 스캔 BETWEEN | 2.1639초 | 43.277ms | CSV 1,000,000 rows 전체 스캔 |
| 성능 차이 | 약 191.7배 빠름 | - | B+Tree 기준 |


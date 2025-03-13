# 📌포트폴리오

## 로그인 - 채팅 - 모니터링 서버 구조
<img width="1564" alt="image" src="https://github.com/user-attachments/assets/dec3547b-78cf-46f5-92bb-b8e86f5e472f" />
---

## 📂 주요 폴더 설명

### 🔹 서버 관련
- **`ChatServer-Redis_Multi`** : 멀티스레드 기반 채팅 서버  
- **`ChatServer-Redis_Single`** : 싱글스레드 기반 채팅 서버  
- **`LoginServer`** : 로그인 서버  
- **`MonitorServer`** : 서버 모니터링 기능을 제공하는 모니터링 서버  

### 🔹 클라이언트 관련
- **`ChatDistributeClient`** : 채팅 서버의 섹터 분포를 확인하는 클라이언트  
- **`ChatDummy`** : 로그인 및 채팅 더미 클라이언트 (이중 로그인 및 타임아웃 검증 목적)  

### 🔹 네트워크 라이브러리
- **`NetClient`** :  
  서버와 클라이언트가 서로 다른 LAN에 존재한다고 가정하고 인코딩을 진행하는 클라이언트 라이브러리 (`ChatDummy` 구현 시 사용)  
- **`LanClient`** :  
  같은 LAN 내에서 통신할 때 사용하는 클라이언트 라이브러리  
  (채팅/로그인 서버와 모니터링 서버 간 통신 시 사용)  

### 🔹 스케줄링 & 프로파일링
- **`Scheduler`** : IOCP에서 프레임을 구현할 때 `PQCS`를 IOCP 큐에 게시하는 스케줄러 스레드 구현체  
- **`MultiThreadProfiler`** : 포트폴리오 문서에서 테스트 결과를 도출할 때 사용한 멀티스레드 프로파일러  

### 🔹 게임 서버 라이브러리
- **`GameServerLib`** :  
  병렬 콘텐츠 및 프레임 콘텐츠를 추상화하는 기능과 네트워크 라이브러리를 포함한 게임 서버 라이브러리  

### 🔹 자료구조 관련
- **`DataStructure`** :  
  - 락프리 큐, 락프리 스택  
  - Embedded Linked List (싱글 채팅 서버 및 메모리 누수 방지에 사용)  
  - TlsObjectPool

### 🔹 로깅 & 파싱
- **`Logger`** :  
  모든 서버 및 클라이언트에서 공용으로 사용하는 `LoggerMt.dll`, `LoggerMt.lib` 구현  
- **`TextParser`** :  
  모든 서버 및 클라이언트에서 설정 파일(`ConfigFile`)을 읽을 때 사용하는 `TextParser.dll`, `TextParser.lib` 구현  

### 🔹 DB & REDIS 관련
- **`MySqlUtil`** :  
  MySQL 쿼리 문자열 조합 기능 및 필요한 MySQL 관련 파일 포함  
- **`RedisUtil`** :  
  `cpp_redis` 라이브러리를 기반으로 Redis 접속 및 기능 구현  

### 🔹 버퍼 관리
- **`SeriaLizeBuffer_AND_RingBuffer`** :  
  링 버퍼 및 직렬화 버퍼 구현  

### 🔹 서버 모니터링
- **`PdhMonitor`** : 서버 성능을 모니터링하는 구현체  
  - 서버 전체 및 프로세스별 **CPU 사용률**  
  - **메모리 Private Byte 사용량**  
  - **논페이지드 풀 (Nonpaged Pool) 사용량**  

### 🔹 기타 공용 코드
- **`Common`** :  
  서버 및 클라이언트에서 공용으로 사용하는 코드  

### 🔹 테스트 코드
- TlsObjectPool Alloc,Free VS malloc,free 성능 비교 테스트 코드 -> TlsMalloc 폴더
- LockFreeQueue Vs Critical Section Queue Vs SpinLock Queue 성능 비교 테스트 코드 -> LockFreeVersusSpinLock

### 🔹 길찾기 검증 테스트
- PathFInd_GUI_TEST
---

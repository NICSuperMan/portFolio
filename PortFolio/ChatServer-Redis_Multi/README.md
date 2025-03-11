# 멀티 로직스레드 채팅서버

## ✨ 특징  
- **전체 맵 (2500섹터) → 4분할**  
- **프레임마다 4개의 스레드로 병렬 분할 처리**  

---

## 🔹 스레드 구조  
<img width="1910" alt="image" src="https://github.com/user-attachments/assets/8feb19ef-0be1-47c2-92c4-866b32c13008" />

## 🔹 섹터 처리 방식  
<img width="1907" alt="image" src="https://github.com/user-attachments/assets/4b95e700-4f71-442d-be78-86c131e2cf5f" />

---

## 📂 주요 파일 설명  
- **`LoginChatServer.h/.cpp`**  
  - `GameServer`를 상속한 **채팅 IOCP** 구현  
- **`RecvLayer.h/.cpp`**  
  - **메시지 수신 시** `JOB` 구조체를 적절히 초기화 후 **전역 JOB 큐에 삽입**  
- **`Job.h/.cpp`**  
  - **`SECTOR_MOVE_JOB`, `BROADCAST_JOB`**의 주요 로직 구현  
- **`JobProcessLayer.h/.cpp`**  
  - **프레임 스레드가 매 프레임마다 수행하는 로직**  
  - **전역 JOB 큐의 모든 JOB을 사분면 JOB 큐에 분배**  
- **`DuplicateLogin.h/.cpp`**  
  - **같은 계정 중복 로그인 시, 먼저 접속한 클라이언트를 강제 종료**  
- **`Sector.h/.cpp`**  
  - **클라이언트 섹터 삽입/삭제, 가시거리 섹터 탐색** 로직 구현  
- **`SCCContents.h/.cpp`**  
  - **서버 → 클라이언트 패킷 생성 함수 모음**  
- **`LoginChatConfig.txt`**  
  - **설정 파일**  

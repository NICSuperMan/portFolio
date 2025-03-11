# 채팅서버 분포 클라이언트
특징 : 전체 맵(2500섹터)를 4분할하고 프레임마다 4개의 스레드로 병렬 분할처리

# 스레드 구조
<img width="1910" alt="image" src="https://github.com/user-attachments/assets/8feb19ef-0be1-47c2-92c4-866b32c13008" />

# 섹터 처리 방식
<img width="1907" alt="image" src="https://github.com/user-attachments/assets/4b95e700-4f71-442d-be78-86c131e2cf5f" />

## 📂 주요 파일 설명  
- **`LoginChatServer.h/.cpp`** : `GameServer`를 상속한 **채팅 IOCP** 구현
- **`RecvLayer.h/.cpp`** : `메시지 수신`시 JOB구조체 적절히 초기화 후 전역 JOB Q에 삽입 구현
- **`Job.h.cpp`** : `SECTOR_MOVE_JOB,BROADCAST_JOB`의 주요 로직 구현
- **`JobProcessLayer.h/.cpp`** : 프레임 스레드가 `프레임 마다` 수행하는 로직(전역 JOB Q의 모든 JOB을 사분면 JOB Q에 분배) 구현
- **`DuplicateLogin.h/.cpp`** : 같은 회원 이중 로그인시 먼저 들어온 클라를 서버에서 끊는 로직 구현
- **`Sector.h/.cpp`** : 클라이언트 섹터 삽입,삭제, 가시거리 섹터 탐색 로직 구현
- **`SCCContents.h/.cpp`** : 서버에서 클라에게 보내는 패킷 만드는 함수 모음 
- **`LoginChatConfig.txt`** : 설정 파일  

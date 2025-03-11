# 🏷️ 모니터링 서버  
✅ **일정 주기로 MYSQL에 모니터링 데이터 비동기 저장**  
✅ **채팅, 로그인 서버로부터 모니터링 데이터 수신 시 모니터링 클라이언트에 브로드캐스트**  

---

<img width="1528" alt="image" src="https://github.com/user-attachments/assets/43d47e41-f31a-4243-ae49-90666acbe70c" />

---

## 📂 주요 파일 설명  
- **`MonitorLanServer.h/.cpp`**  
  - **채팅, 로그인 서버와 통신하는 IOCP**  
  - 같은 LAN 내 서버 간에는 패킷 인코딩 없이 통신  
  - (`GameServerLib`, `NetClient` 등은 패킷 인코딩 후 통신)  
- **`MonitorNetServer.h/.cpp`**  
  - **모니터링 클라이언트와 통신하는 IOCP**  
  - 채팅·로그인 서버와 달리 **패킷 인코딩 후 통신** (`NetClient` 상속)  
- **`ChatData.h`**  
  - **채팅 서버에서 전송한 모니터링 데이터를 통계 처리**  
  - 주기적으로 **DB에 저장**  
- **`LoginData.h`**  
  - **로그인 서버에서 전송한 모니터링 데이터를 통계 처리**  
  - 주기적으로 **DB에 저장**  
- **`ServerCommonData.h`**  
  - **서버 PC의 모니터링 데이터 저장**  
  - (가용 메모리, NIC Send/Recv, Non-Paged Pool 등)  
  - 주기적으로 **DB에 저장**  
- **`MonitorDBWriteThread.h`**  
  - 일정 주기로 `*Data.h`의 데이터를 **DB에 저장하는 쓰레드 구현**  
  - `DBWriteThreadBase` 상속 (`MySqlUtil` 폴더 참고)  

---

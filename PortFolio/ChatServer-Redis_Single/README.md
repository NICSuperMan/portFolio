# 🏷️ 싱글 로직 스레드 채팅서버  
✅ **로그인 인증 처리 제외, 모든 로직 싱글 스레드 처리**  

---

## ✨ 특징  
- **전체 맵 (2500섹터) → 4분할**  
- **프레임마다 4개의 스레드로 병렬 분할 처리**  

---

## 🔹 스레드 구조  
<img width="1703" alt="image" src="https://github.com/user-attachments/assets/694c5b77-1c15-4163-b656-be31ade02ee1" />

## 🔹 섹터 처리 방식  
<img width="1427" alt="image" src="https://github.com/user-attachments/assets/d35d4d12-c1ec-41de-944b-ef4acc79c6ea" />

---

## 📂 주요 파일 설명  
- **`LoginChatServer.h/.cpp`**  
  - `GameServer`를 상속한 **채팅 IOCP** 구현  
- **`LoginContents.h/.cpp`**  
  - **로그인 시** **레디스 인증 처리** 구현  
  - `ParallelContents` 상속  
- **`ChatContents.h/.cpp`**  
  - **프레임 스레드의 `섹터 이동`, `채팅 브로드캐스팅` 처리**  
  - `SerialContents` 상속  
- **`Sector.h/.cpp`**  
  - **클라이언트 섹터 삽입/삭제, 가시거리 섹터 탐색** 로직 구현  
- **`SCCContents.h/.cpp`**  
  - **서버 → 클라이언트 패킷 생성 함수 모음**  
---

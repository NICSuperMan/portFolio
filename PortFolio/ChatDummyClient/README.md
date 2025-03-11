# 로그인 - 채팅서버 더미 클라이언트

## 🎥 작동 영상  
[▶ YouTube 영상](https://www.youtube.com/watch?v=pMuY1T9bwNI)

## 주요 작동 구조  
<img width="585" alt="image" src="https://github.com/user-attachments/assets/b8d15c62-8675-4cda-8940-2c06b997c6d2" />

## 📂 주요 파일 설명  
- **`AccoutInfo.h/.cpp`** :  
  - 접속할 클라이언트의 **AccountNo, ID, NickName**을 읽어오고 초기화  
- **`ID1,2,3,4.txt`** :  
  - `AccountInfo`에서 사용하는 **클라이언트 정보 파일**  
- **`Lyrics.h/.cpp`** :  
  - 클라이언트들이 사용할 **채팅 샘플**을 읽어오고 초기화  
- **`ChatList.txt`** :  
  - `Lyrics`에서 사용하는 **채팅 메시지 목록**  
- **`ChatDumy.h/.cpp`** :  
  - **채팅 서버와 연결할 더미 클라이언트 IOCP** (`NetClient` 상속)  
- **`LoginDummy.h/.cpp`** :  
  - **로그인 서버와 연결할 더미 클라이언트 IOCP** (`NetClient` 상속)  
- **`RecvJob.h/.cpp`** :  
  - **로그인, 채팅 더미 IOCP**가 **Recv 완료 시 전역 큐에 전달할 Job 구조체**  
- **`MakePacket.h/.cpp`** :  
  - **클라이언트가 보낼 패킷을 생성하는 함수 모음**  
- **`DummyConfig.h/.cpp`** :  
  - **더미 클라이언트의 환경 설정을 관리하는 파일**  

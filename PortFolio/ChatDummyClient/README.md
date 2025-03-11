# 로그인 - 채팅서버 더미 클라이언트

## 🎥 작동 영상  
[▶ YouTube 영상](https://www.youtube.com/watch?v=pMuY1T9bwNI)

## 주요 작동 구조
<img width="585" alt="image" src="https://github.com/user-attachments/assets/b8d15c62-8675-4cda-8940-2c06b997c6d2" />


## 📂 주요 파일 설명  
- **`AccoutInfo.h/.cpp`** : 접속할 클라의 **AccountNo, ID, NickName**을 읽어오고 초기화
- **`ID1,2,3,4.txt`** : AccountInfo 사용파일
- **`Lyrics.h/.cpp`** : 클라들이 사용할 채팅샘플을 읽어오고 초기화
- **`ChatList.txt`** : Lyrics 사용파일
- **`ChatDumy.h/.cpp`** : 채팅 서버와 연결할 클라이언트용 IOCP(NetClient 상속)
- **`LoginDummy.h/.cpp`** :  채팅 서버와 연결할 클라이언트용 IOCP(NetClient 상속)
- **`RecvJob.h/.cpp`** : 로그인, 채팅 더미 IOCP들이 Recv완료통지시 전역 큐에 전달할 Job 구조체
- **`MakePacket.h/.cpp`** : 클라가 보낼 패킷을 만드는 함수 모음
- **`DummyConfig.h/.cpp`** : 설정파일

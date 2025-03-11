# 채팅서버 분포 클라이언트

## 🎥 작동 영상  
[▶ YouTube 영상](https://www.youtube.com/watch?v=5TrDj1DaVts)

## 📂 주요 파일 설명  
- **`ChatDitributeClient.h/.cpp`** : `NetClient`를 상속한 **모니터링 클라이언트 IOCP** 구현  
- **`HeartBeatUpdate.h/.cpp`** :  
  - `Config` 파일에서 지정한 간격마다 **채팅 서버에 섹터 인구 분포 요청 메시지** 송신  
  - 해당 기능을 **스케줄러에 등록**하여 주기적으로 실행  

## ⚙️ 주요 Config 설정  
- **`ALERT_NUM`** :  
  - **비정상적인 클라이언트 인구** 기준 값  
  - 해당 숫자 이상인 섹터는 **검정색으로 렌더링**  
- **`REQUEST_INTERVAL`** :  
  - **스케줄러가 `HeartBeatUpdate`를 IOCP 큐에 게시하는 간격**  

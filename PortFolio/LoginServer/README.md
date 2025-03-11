# 로그인 서버

## ✨ 특징  
- IO_BOUND 특성을 활용한 스레드 설계

## 🔹 스레드 구조  
<img width="1900" alt="image" src="https://github.com/user-attachments/assets/835d7496-5c40-4b8b-94bc-cc3e2d23d4e9" />
<img width="1838" alt="image" src="https://github.com/user-attachments/assets/817084e2-d894-45ec-91eb-78c39e75c5da" />

---
## 📂 주요 파일 설명  
- **`LoginServer.h/.cpp`**  
  - `GameServer`를 상속한 **로그인 IOCP** 구현  
- **`LoginContents.h/.cpp`**  
  - **로그인 시** **MYSQL, 레디스 인증 처리** 구현  
  - `ParallelContents` 상속  

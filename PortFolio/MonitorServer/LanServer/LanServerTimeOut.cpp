#include <Winsock2.h>
#include "LanServerTimeOut.h"

LanServerTimeOut::LanServerTimeOut(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LanServer* pLanServer)
	:UpdateBase{ tickPerFrame,hCompletionPort,pqcsLimit }, pLanServer_{ pLanServer }
{
}

LanServerTimeOut::~LanServerTimeOut()
{
}

// Release 플래그 변경 -> time = 0 ->락프리 스택에 Push -> id 변경(Init) ->  AcceptEx -> 완료통지 이후 time을 현재시간으로 변경
void LanServerTimeOut::Update_IMPL()
{
	ULONGLONG currentTime = GetTickCount64();
	for (int i = 0; i < pLanServer_->maxSession_; ++i)
	{
		LanSession* pSession = pLanServer_->pSessionArr_ + i;

		// RELEASE_된지 좀 된 대다수의 세션을 걸러내기 위해서 최상단
		// 세션이 Release되기 직전에 비순차적 명령어 실행으로 RefCnt를 미리읽어서 해당 if문을 통과하더라도 Disconnect에서 걸러짐, 운좋게 통과하더라도 이미 끊기기 직전인 애가 끊긴거라서 상관없음
		if ((pSession->refCnt_ & LanSession::RELEASE_FLAG) == LanSession::RELEASE_FLAG)
			continue;

		// 세션이 재활용 되었는데 id를 현재값을 읽은 경우 ReleaseSession에서 LastRecvTime을 0으로 바꾸고 초기화에서 해당변수를 현재시간으로 바꾸기때문에 0 혹은 현재시간을 읽는다 
		ULONGLONG id = pSession->id_;
		ULONGLONG lastRecvTime = pSession->lastRecvTime;

		if (lastRecvTime == 0)
			continue;

		if (currentTime < lastRecvTime + pLanServer_->TIME_OUT_MILLISECONDS_) // 정밀할 필요가 없어서
			continue;

		pLanServer_->Disconnect(id);
	}
}

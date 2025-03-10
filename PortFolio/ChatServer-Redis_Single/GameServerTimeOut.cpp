#include <Winsock2.h>
#include "GameSession.h"
#include "GameServer.h"
#include "GameServerTimeOut.h"

GameServerTimeOut::GameServerTimeOut(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LONG sessionTimeOut, LONG authUserTimeOut, GameServer* pGameServer)
	:UpdateBase{ tickPerFrame,hCompletionPort,pqcsLimit }, sessionTimeOut_{ sessionTimeOut }, authUserTimeOut_{ authUserTimeOut }, pGameServer_{ pGameServer }
{
}

GameServerTimeOut::~GameServerTimeOut()
{
}

void GameServerTimeOut::Update_IMPL()
{
	ULONGLONG currentTime = GetTickCount64();
	for (int i = 0; i < pGameServer_->maxSession_; ++i)
	{
		GameSession* pSession = pGameServer_->pSessionArr_ + i;

		long refCnt = InterlockedIncrement(&pSession->refCnt_);

		// 이미 RELEASE 진행중이거나 RELEASE된 경우
		if ((refCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
		{
			if (InterlockedDecrement(&pSession->refCnt_) == 0)
				pGameServer_->ReleaseSession(pSession);
			continue;
		}

		ULONGLONG lastRecvTime = pSession->lastRecvTime;

		if (!pSession->bLogin_ && currentTime >= lastRecvTime + sessionTimeOut_)
		{
			pGameServer_->Disconnect(pSession->id_);
		}
		else if (currentTime >= lastRecvTime + authUserTimeOut_) 
		{
			pGameServer_->Disconnect(pSession->id_);
		}

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			pGameServer_->ReleaseSession(pSession);
	}
}

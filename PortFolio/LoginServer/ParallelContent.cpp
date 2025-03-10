#include <WinSock2.h>
#include "GameServer.h"
#include "Packet.h"
#include "ParallelContent.h"

ParallelContent::ParallelContent(GameServer* pGameServer)
	:ContentsBase{ false,pGameServer }
{}

ParallelContent::~ParallelContent()
{
}

void ParallelContent::WorkerHanlePacketAtRecvLoop(Packet* pPacket, GameSession* pSession)
{
	pSession->recvMsgQ_.Enqueue(pPacket);

#pragma warning(disable : 26815)
	Packet* pTargetPacket = pSession->recvMsgQ_.Dequeue().value();
#pragma warning(default : 26815)

	OnRecv(pPacket, pGameServer_->GetPlayer(pSession));
	PACKET_FREE(pTargetPacket);
}

void ParallelContent::RequestFirstEnter(void* pPlayer)
{
	GameSession* pSession = pGameServer_->GetSession(pPlayer);
	InterlockedExchangePointer((PVOID*)&pSession->pCurContent, (ContentsBase*)this);
	OnEnter(pPlayer);
}

void ParallelContent::RequestEnter(const bool bPrevContentsIsSerialize, GameSession* pSession)
{
	InterlockedExchangePointer((PVOID*)&pSession->pCurContent, (ContentsBase*)this);
	OnEnter(pGameServer_->GetPlayer(pSession));
	
	// OnEnter가 앞인 이유는 만약 OnEnter가 iocnt를 감소시킨 뒤 진행된다면 그사이에 라이브러리의 ReleaseSession이 호출될 수 잇음
	// 결국 해당클래스의 ReleaseSessionPost에서 OnLeave호출후 스택에 푸시함
	// 해당 함수는 멀티컨텐츠의 가상함수이기 때문에 OnEnter와 OnLeave가 동시에 호출되는 위험 && 재활용을
	// 없애기 위해 OnEnter호출후 IoCnt 내림
	if (bPrevContentsIsSerialize)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
		{
			ReleaseSession_AT_ONCE_NOT_CALL_ONLEAVE(pSession);
			return;
		}
	}
}

void ParallelContent::ReleaseSession(GameSession* pSession)
{
	LONG size = pSession->recvMsgQ_.GetSize();
	// 세션의 수신 메시지 큐 비움
	for (LONG i = 0; i < size; ++i)
	{
#pragma warning(disable : 26815)
		Packet* pPacket = pSession->recvMsgQ_.Dequeue().value();
#pragma warning(default : 26815)
		PACKET_FREE(pPacket);
	}

	OnLeave(pGameServer_->GetPlayer(pSession));

	pGameServer_->idxStack_.Push((short)(pSession - pGameServer_->pSessionArr_));
	InterlockedIncrement(&pGameServer_->disconnectTPS_);
	InterlockedDecrement(&pGameServer_->lSessionNum_);
}

void ParallelContent::RegisterLeave(void* pPlayer, int nextContent)
{
	OnLeave(pPlayer);
	GameSession* pSession = pGameServer_->GetSession(pPlayer);
	GetContentsPtr(nextContent)->RequestEnter(bSerial_, pSession);
}


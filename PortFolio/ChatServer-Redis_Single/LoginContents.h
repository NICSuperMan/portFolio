#pragma once
#include <cpp_redis\cpp_redis>
#include "GameServer.h"
#include "ParallelContent.h"
#include "CommonProtocol.h"

class LoginContents : public ParallelContent
{
public:
	LoginContents(GameServer* pGameServer);
	// ContentsBase overridng 
	virtual void OnEnter(void* pPlayer) override; // 플레이어(세션) 컨텐츠 입장
	virtual void OnLeave(void* pPlayer) override; // 플레이어(세션) 컨텐츠 퇴장
	virtual void OnRecv(Packet* pPacket, void* pPlayer) override; // Packet Handler
	void CS_CHAT_REQ_LOGIN(void* pPlayer, Packet* pPacket);
};

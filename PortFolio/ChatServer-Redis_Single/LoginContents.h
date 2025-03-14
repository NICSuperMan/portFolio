#pragma once
#include <cpp_redis\cpp_redis>
#include <GameServerLib/GameServer.h>
#include <GameServerLib/ParallelContent.h>
#include <Common/CommonProtocol.h>

class LoginContents : public ParallelContent
{
public:
	LoginContents(GameServer* pGameServer);
	// ContentsBase overridng 
	virtual void OnEnter(void* pPlayer) override; // �÷��̾�(����) ������ ����
	virtual void OnLeave(void* pPlayer) override; // �÷��̾�(����) ������ ����
	virtual void OnRecv(Packet* pPacket, void* pPlayer) override; // Packet Handler
	void CS_CHAT_REQ_LOGIN(void* pPlayer, Packet* pPacket);
};

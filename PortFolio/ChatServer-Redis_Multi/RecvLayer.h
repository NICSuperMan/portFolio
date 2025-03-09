#pragma once
#include "GameServer.h"
#include "ParallelContent.h"
struct Player;
class RecvLayer : public ParallelContent
{
public:
	RecvLayer(GameServer* pGameServer);
	virtual ~RecvLayer();
	virtual void OnEnter(void* pPlayer);
	virtual void OnLeave(void* pPlayer);
	virtual void OnRecv(Packet* pPacket, void* pPlayer);
	void CS_CHAT_REQ_LOGIN(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, char* pSessionKey, Player* pPlayer);
	void CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, Player* pPlayer);
	void CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer);
}; 
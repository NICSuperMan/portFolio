#include <WinSock2.h>
#include <Common/CommonProtocol.h>
#include "HeartBeatUpdate.h"

RequestSectorInfo::RequestSectorInfo(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, ChatDistributionClient* pClient)
	:UpdateBase{ tickPerFrame,hCompletionPort,pqcsLimit }, pClient_{ pClient }
{
}

RequestSectorInfo::~RequestSectorInfo()
{
}

void RequestSectorInfo::Update_IMPL()
{
	SmartPacket sp = PACKET_ALLOC(Net);
	*sp << (WORD)en_PACKET_CS_CHAT_REQ_SECTOR_INFO;
	pClient_->SendPacket(pClient_->ClientSessionID_, sp);
}

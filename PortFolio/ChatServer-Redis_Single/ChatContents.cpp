#include "Winsock2.h"
#include "SCCContents.h"
#include "GameServer.h"
#include "ChatContents.h"
#include "en_ChatContentsType.h"
#include "Sector.h"
#include "LoginChatServer.h"
#include "Packet.h"
#include "CommonProtocol.h"

#include <memory.h>

ChatContents::ChatContents(const DWORD tickPerFrame, const HANDLE hCompletionPort, const LONG pqcsLimit, GameServer* pGameServer)
	:SerialContent{ tickPerFrame,hCompletionPort,pqcsLimit,pGameServer }, playerList{ offsetof(Player,contentLink) }
{
}

void ChatContents::OnEnter(void* pPlayer)
{
	Player* pAuthPlayer = (Player*)pPlayer;
	playerList.push_back(pPlayer);

	// 로그인 알림으로서 세션 타임아웃을 벗어남
	pGameServer_->SetLogin(pAuthPlayer->sessionId_);
	if (pAuthPlayer->bMonitoringLogin_)
	{
		InterlockedIncrement(&pGameServer_->lPlayerNum_);
		return;
	}

	const auto& iter = loginPlayerMap.find(pAuthPlayer->accountNo_);
	if (iter != loginPlayerMap.end())
	{
		pGameServer_->Disconnect(iter->second->sessionId_);
		loginPlayerMap.erase(iter);
	}

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(1, pAuthPlayer->accountNo_, sp);
	pGameServer_->SendPacket(pAuthPlayer->sessionId_, sp.GetPacket());

	loginPlayerMap.insert(std::make_pair(pAuthPlayer->accountNo_, pAuthPlayer));

	InterlockedIncrement(&pGameServer_->lPlayerNum_);
}

void ChatContents::OnLeave(void* pPlayer)
{
	Player* pLeavePlayer = (Player*)pPlayer;
	playerList.remove(pPlayer);

	if (pLeavePlayer->bMonitoringLogin_)
	{
		EnterCriticalSection(&Player::MonitorSectorInfoCs);
		if (Player::MonitoringClientSessionID == pLeavePlayer->sessionId_)
			Player::MonitoringClientSessionID = MAXULONGLONG;

		LeaveCriticalSection(&Player::MonitorSectorInfoCs);
	}

	// 세션만 생성되고 로그인 안하다가 자의로 끊거나 타임아웃된경우
	if (!pLeavePlayer->bLogin_)
		return;

	// 이중 로그인 처리 -> 만약 같은 AccountNo의 다른 세션이 접속함에 따라 Disconnect 경우라면 bLogin이 true이고 loginPlayerMap에서 찾앗을때 없을것이다.
	auto iter = loginPlayerMap.find(pLeavePlayer->accountNo_);
	if (iter->second == pLeavePlayer)
	{
		loginPlayerMap.erase(iter); // 이중로그인으로 인해서 끊긴 클라가 아닌 경우
	}

	// 로그인햇다면 이미 이동 메시지를 보내고 등록된 좌표에 해당하는 섹터에 등록햇을것이므로 삭제한다.
	if (pLeavePlayer->bRegisterAtSector_)
		RemoveClientAtSector(pLeavePlayer->sectorX_, pLeavePlayer->sectorY_, pLeavePlayer);

	InterlockedDecrement(&pGameServer_->lPlayerNum_);
}


void ChatContents::OnRecv(Packet* pPacket, void* pPlayer)
{
	Player* pRecvPlayer = (Player*)pPlayer;
	WORD Type;
	try
	{
		(*pPacket) >> Type;
		switch ((en_PACKET_TYPE)Type)
		{
		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			INT64 accountNo;
			WORD sectorX;
			WORD sectorY;
			(*pPacket) >> accountNo >> sectorX >> sectorY;
			if(!pPacket->IsBufferEmpty())
			{
				pGameServer_->Disconnect(pRecvPlayer->sessionId_);
				return;
			}
			CS_CHAT_REQ_SECTOR_MOVE(accountNo, sectorX, sectorY, pRecvPlayer);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			INT64 accountNo;
			WORD messageLen;
			(*pPacket) >> accountNo >> messageLen;
			WCHAR* pMessage = (WCHAR*)pPacket->GetPointer(messageLen);
			if (!pMessage || !pPacket->IsBufferEmpty())
			{
				pGameServer_->Disconnect(pRecvPlayer->sessionId_);
				return;
			}
			CS_CHAT_REQ_MESSAGE(accountNo, messageLen, pMessage, pRecvPlayer);
			break;
		}
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;

		case en_PACKET_CS_CHAT_MONITOR_CLIENT_LOGIN:  // 로그인컨텐츠에 와야함
			pGameServer_->Disconnect(pRecvPlayer->sessionId_);
			break;

		case en_PACKET_CS_CHAT_REQ_SECTOR_INFO:
			CS_CHAT_REQ_SECTORINFO(pRecvPlayer->sessionId_);
			break;

		default:
			pGameServer_->Disconnect(pGameServer_->GetSessionID(pPlayer));
			break;
		}
	}
	catch (int errCode)
	{
		if (errCode == ERR_PACKET_EXTRACT_FAIL)
		{
			pGameServer_->Disconnect(pRecvPlayer->sessionId_);
		}
		else if (errCode == ERR_PACKET_RESIZE_FAIL)
		{
			// 지금은 이게 실패할리가 없음 사실 모니터링클라 접속안하면 리사이즈조차 아예 안일어날수도잇음, 원래는 Disconnect가 맞음
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			pGameServer_->Disconnect(pRecvPlayer->sessionId_);
		}
	}
	InterlockedIncrement(&static_cast<LoginChatServer*>(pGameServer_)->UPDATE_CNT_TPS);
}

void ChatContents::ProcessEachPlayer()
{
}

void ChatContents::CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, Player* pPlayer)
{
	if (!pPlayer->bLogin_)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	if (pPlayer->accountNo_ != accountNo)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 클라가 유효하지 않은 좌표로 요청햇다면 끊어버린다
	if (IsNonValidSector(sectorX, sectorY))
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 최초등록이 아니라면 현재 섹터에서 제거
	if (!pPlayer->bRegisterAtSector_)
		pPlayer->bRegisterAtSector_ = true;
	else
		RemoveClientAtSector(pPlayer->sectorX_, pPlayer->sectorY_, pPlayer);

	// 새로운 섹터에 등록
	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;
	RegisterClientAtSector(sectorX, sectorY, pPlayer);

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(accountNo, sectorX, sectorY, sp);
	pGameServer_->EnqPacket(pPlayer->sessionId_, sp.GetPacket());
}

void ChatContents::CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer)
{
	if (!pPlayer->bLogin_)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}
	if (pPlayer->accountNo_ != accountNo)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 패킷 만들기
	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, sp);
	SECTOR_AROUND sectorAround;

	// 본인 위치 섹터 + 주위 8섹터 구하기
	GetSectorAround(pPlayer->sectorX_, pPlayer->sectorY_, &sectorAround);
	// 채팅 내용 BroadCasting
	SendPacket_AROUND(&sectorAround, sp);
}

extern struct SectorArray sectors;

class SectorInfoPacket
{
	Packet* pPacket_;

public:
	SectorInfoPacket(GameServer* pGameServer)
		:pGameServer_{ pGameServer }
	{
		pPacket_ = new Packet;
		pPacket_->Clear<Net>();
		pPacket_->IncreaseRefCnt();
	}

	void SendInfo(ULONGLONG sessionID)
	{
		pPacket_->Clear<Net>();
		for (int y = 0; y < NUM_OF_SECTOR_VERTICAL; ++y)
		{
			for (int x = 0; x < NUM_OF_SECTOR_HORIZONTAL; ++x)
			{
				*pPacket_ << (char)sectors.listArr[y][x].size_;
				if ((char)sectors.listArr[y][x].size_ < 0)
					__debugbreak();
			}
		}
		pPacket_->IncreaseRefCnt();
		pGameServer_->SendPacket(sessionID, pPacket_);
	}

	~SectorInfoPacket()
	{
		if (pPacket_->DecrementRefCnt() == 0)
			delete pPacket_;
	}

private:
	GameServer* pGameServer_;

};

std::unique_ptr<SectorInfoPacket> pSectorInfo;

void ChatContents::CS_CHAT_REQ_SECTORINFO(ULONGLONG sessionID)
{
	static bool g_bFirst = false;
	if (!g_bFirst)
	{
		pSectorInfo = std::make_unique<SectorInfoPacket>(pGameServer_);
		g_bFirst = true;
	}

	pSectorInfo->SendInfo(sessionID);
}

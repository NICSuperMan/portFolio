#include "LoginContents.h"
#include "Player.h"
#include "SCCContents.h"
#include "en_ChatContentsType.h"
#include "LoginChatServer.h"
#include "RedisClientWrapper.h"

using namespace cpp_redis;

#pragma warning(disable : 26495)
LoginContents::LoginContents(GameServer* pGameServer)
	:ParallelContent{pGameServer}
{
}
#pragma warning(default : 26495)

void LoginContents::OnEnter(void* pPlayer)
{
	((Player*)(pPlayer))->bLogin_ = false;
	((Player*)(pPlayer))->bRegisterAtSector_ = false;
	((Player*)(pPlayer))->bMonitoringLogin_ = false;
}

void LoginContents::OnLeave(void* pPlayer)
{
}


void LoginContents::OnRecv(Packet* pPacket, void* pPlayer)
{
	Player* pAuthPlayer = (Player*)pPlayer;
	WORD Type;
	try
	{
		(*pPacket) >> Type;
		switch ((en_PACKET_TYPE)Type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
			CS_CHAT_REQ_LOGIN(pPlayer, pPacket);
			break;

		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;

		case en_PACKET_CS_CHAT_MONITOR_CLIENT_LOGIN:
			EnterCriticalSection(&Player::MonitorSectorInfoCs);
			pAuthPlayer->bLogin_ = true;
			pAuthPlayer->bMonitoringLogin_ = true;
			if (Player::MonitoringClientSessionID != MAXULONGLONG)
			{
				pGameServer_->Disconnect(Player::MonitoringClientSessionID);
			}
			Player::MonitoringClientSessionID = pAuthPlayer->sessionId_;
			LeaveCriticalSection(&Player::MonitorSectorInfoCs);
			RegisterLeave(pPlayer, (int)en_ChatContentsType::CHAT);
			break;

		default:
			__debugbreak();
			break;
		}
	}
	catch (int errCode)
	{
		if (errCode == ERR_PACKET_EXTRACT_FAIL)
		{
			pGameServer_->Disconnect(pGameServer_->GetSessionID(pPlayer));
		}
		else if (errCode == ERR_PACKET_RESIZE_FAIL)
		{
			// 지금은 이게 실패할리가 없음 사실 모니터링클라 접속안하면 리사이즈조차 아예 안일어날수도잇음
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			__debugbreak();
		}

		if (Type == en_PACKET_CS_CHAT_MONITOR_CLIENT_LOGIN)
		{
			LeaveCriticalSection(&Player::MonitorSectorInfoCs);
		}
	}
	InterlockedIncrement(&static_cast<LoginChatServer*>(pGameServer_)->UPDATE_CNT_TPS);
}


void LoginContents::CS_CHAT_REQ_LOGIN(void* pPlayer, Packet* pPacket)
{
	Player* pLoginPlayer = (Player*)pPlayer;

	ULONGLONG sessionID = pGameServer_->GetSessionID(pPlayer);
	if (pGameServer_->lPlayerNum_ >= Player::MAX_PLAYER_NUM)
	{
		pGameServer_->Disconnect(sessionID);
		return;
	}

	// 패킷 언마샬링
	INT64 accountNo;
	(*pPacket) >> accountNo;
	WCHAR* pID = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
	WCHAR* pNickName = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
	char* pSessionKey = pPacket->GetPointer(Player::SESSION_KEY_LEN);


	// 인증 토큰 레디스에서 동기로 읽어오기
	client* pClient = GetRedisClient();
	const auto& temp = pClient->get(std::to_string(accountNo));
	pClient->sync_commit();
	const auto& ret = temp._Get_value();

	// 레디스에 세션키가 없거나, 레디스에 저장된 세션키와 다르다면 로그인 실패
	if (ret.is_null() || memcmp(pSessionKey, ret.as_string().c_str(), Player::SESSION_KEY_LEN) != 0)
	{
		pGameServer_->Disconnect(sessionID);
		return;
	}

	// 플레이어 초기화 및 로그인 처리
	pLoginPlayer->bLogin_ = true;
	pLoginPlayer->accountNo_ = accountNo;
	wcscpy_s(pLoginPlayer->ID_, Player::ID_LEN, pID);
	wcscpy_s(pLoginPlayer->nickName_, Player::NICK_NAME_LEN, pNickName);
	RegisterLeave(pPlayer, (int)en_ChatContentsType::CHAT);
}


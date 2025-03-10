#include <WinSock2.h>
#include "RecvLayer.h"
#include <Common/CommonProtocol.h>
#include "Player.h"
#include <RedisUtil/cpp_redis\cpp_redis>
#include <RedisUtil/RedisClientWrapper.h>
#include "SCCContents.h"
#include "DuplicateLogin.h"
#include <DataStructure/CLockFreeQueue.h>
#include "Job.h"
#include "Sector.h"


using namespace cpp_redis;

CLockFreeQueue<JOB*> g_jobQ;

RecvLayer::RecvLayer(GameServer* pGameServer)
	:ParallelContent{ pGameServer }
{
}

RecvLayer::~RecvLayer()
{
}

void RecvLayer::OnEnter(void* pPlayer)
{
	((Player*)pPlayer)->bLogin_ = false;
	((Player*)pPlayer)->bRegisterAtSector_ = false;
}

void RecvLayer::OnLeave(void* pPlayer)
{
	Player* pLeavePlayer = (Player*)pPlayer;

	// 세션만 생성되고 로그인 안하다가 자의로 끊거나 세션 타임아웃시간에 걸린 경우
	if (pLeavePlayer->bLogin_)
	{
		DuplicateLogin::RemoveFromLoginPlayerMapWhenLogOut(pLeavePlayer->accountNo_, pLeavePlayer->sessionId_);

		if (pLeavePlayer->bRegisterAtSector_)
		{
			SECTOR_REMOVE_JOB* pRemoveJob = SECTOR_REMOVE_JOB::removeJobPool_.Alloc(pLeavePlayer->sessionId_, pLeavePlayer->sectorX_, pLeavePlayer->sectorY_);
			g_jobQ.Enqueue((JOB*)pRemoveJob);
		}

		InterlockedDecrement(&pGameServer_->lPlayerNum_);
	}
}

void RecvLayer::OnRecv(Packet* pPacket, void* pPlayer)
{
	WORD type;
	try
	{
		*pPacket >> type;
		switch (type)
		{
		case en_PACKET_CS_CHAT_REQ_LOGIN:
		{
			if (pGameServer_->lPlayerNum_ >= Player::MAX_PLAYER_NUM)
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				break;
			}

			INT64 accountNo;
			*pPacket >> accountNo;
			WCHAR* pID = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
			WCHAR* pNickName = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
			char* pSessionKey = (char*)pPacket->GetPointer(Player::SESSION_KEY_LEN);

			if (!pPacket->IsBufferEmpty())
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				return;
			}

			CS_CHAT_REQ_LOGIN(accountNo, pID, pNickName, pSessionKey, (Player*)pPlayer);
			break;
		}

		case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		{
			INT64 accountNo;
			WORD sectorX;
			WORD sectorY;
			*pPacket >> accountNo >> sectorX >> sectorY;

			if (!pPacket->IsBufferEmpty())
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				break;
			}

			if (((Player*)pPlayer)->accountNo_ != accountNo)
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				break;
			}

			CS_CHAT_REQ_SECTOR_MOVE(accountNo, sectorX, sectorY, (Player*)pPlayer);
			break;
		}

		case en_PACKET_CS_CHAT_REQ_MESSAGE:
		{
			INT64 accountNo;
			*pPacket >> accountNo;
			WORD messageLen;
			*pPacket >> messageLen;
			WCHAR* pMessage = (WCHAR*)pPacket->GetPointer(messageLen);

			if (!pPacket->IsBufferEmpty())
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				break;
			}

			if (((Player*)pPlayer)->accountNo_ != accountNo)
			{
				pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
				break;
			}

			CS_CHAT_REQ_MESSAGE(accountNo, messageLen, pMessage, (Player*)pPlayer);
			break;
		}

		// 네트워크 라이브러리 차원에서 타임아웃을 지원하기 때문에 아무것도 안함
		case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
			break;

		default:
			pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
			break;
		}
	}
	catch (int errCode)
	{
		if (errCode == ERR_PACKET_EXTRACT_FAIL)
		{
			pGameServer_->Disconnect(((Player*)pPlayer)->sessionId_);
		}
		else if (errCode == ERR_PACKET_RESIZE_FAIL)
		{
			// 지금은 이게 실패할리가 없음 사실 모니터링클라 접속안하면 리사이즈조차 아예 안일어날수도잇음
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			__debugbreak();
		}
	}
}

void RecvLayer::CS_CHAT_REQ_LOGIN(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, char* pSessionKey, Player* pPlayer)
{
	// 인증 토큰 레디스에서 동기로 읽어오기
	client* pClient = GetRedisClient();
	const auto& temp = pClient->get(std::to_string(accountNo));
	pClient->sync_commit();
	const auto& ret = temp._Get_value();

	// 레디스에 세션키가 없거나, 레디스에 저장된 세션키와 다르다면 로그인 실패
	if (ret.is_null() || memcmp(pSessionKey, ret.as_string().c_str(), Player::SESSION_KEY_LEN) != 0)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// SessionTimeOut을 막기위해 라이브러리에 로그인 처리
	pGameServer_->SetLogin(pPlayer->sessionId_);

	// 락으로 인해 Block가능성 잇음
	DuplicateLogin::PushPlayerToLoginMapAndDisconnectIfSameAccountLoggedIn(accountNo, pPlayer->sessionId_);

	// 플레이어 로그인 처리 및 관련정보 초기화
	pPlayer->bLogin_ = true;
	pPlayer->accountNo_ = accountNo;
	wcscpy_s(pPlayer->ID_, Player::ID_LEN, pID);
	wcscpy_s(pPlayer->nickName_, Player::NICK_NAME_LEN, pNickName);

	SmartPacket sp = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_LOGIN(1, accountNo, sp);
	pGameServer_->SendPacket(pPlayer->sessionId_, sp);
	InterlockedIncrement(&pGameServer_->lPlayerNum_);
}

void RecvLayer::CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, Player* pPlayer)
{
	// 클라가 유효하지 않은 좌표로 요청햇다면 끊어버린다
	if (IsNonValidSector(sectorX, sectorY))
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// 패킷 만들기
	Packet* pCS_CHAT_RES_SECTOR_MOVE = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(accountNo, sectorX, sectorY, pCS_CHAT_RES_SECTOR_MOVE);

	// 이전좌표와 같은 좌표로 SectorMove가 오는경우가 잇어서 일단 예외처리함
	// 그대로 내버려두면 리스트에 같은 세션아이디가 잇는 경우가 생김
	if (sectorX == pPlayer->sectorX_ && sectorY == pPlayer->sectorY_)
	{
		pGameServer_->SendPacket(pPlayer->sessionId_, pCS_CHAT_RES_SECTOR_MOVE);
		return;
	}

	// 도착 섹터에 ADD Job을 삽입한다
	SECTOR_ADD_JOB* pSectorAddJob = SECTOR_ADD_JOB::addJobPool_.Alloc(pCS_CHAT_RES_SECTOR_MOVE, pPlayer->sessionId_, sectorX, sectorY);
	g_jobQ.Enqueue((JOB*)(pSectorAddJob));

	// 이미 등록 되어잇다면 removeJob 삽입
	if (pPlayer->bRegisterAtSector_)
	{
		SECTOR_REMOVE_JOB* pSectorRemoveJob = SECTOR_REMOVE_JOB::removeJobPool_.Alloc(pPlayer->sessionId_, pPlayer->sectorX_, pPlayer->sectorY_);
		g_jobQ.Enqueue((JOB*)(pSectorRemoveJob));
	}
	else
	{
		pPlayer->bRegisterAtSector_ = true;
	}

	// 플레이어 섹터좌표 수정 -> 로직에서 더이상 사용하지 않지만 다음번에 REQ_SECTOR_MOVE 수신시 이전 섹터좌표로 사용한다
	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;
}

void RecvLayer::CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer)
{
	Packet* pCS_CHAT_RES_MESSAGE = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, pCS_CHAT_RES_MESSAGE);

	// 미리 인코딩 하고 보낼때는 SendPacket_ALREADY_ENCODED 사용 -> 그렇지 않으면 여러 스레드가 하나의 직렬화버퍼를 인코딩하려해서 문제 발생
	pCS_CHAT_RES_MESSAGE->SetHeader<Net>();
	BROADCAST_JOB* pJob = BROADCAST_JOB::broadCastJobPool_.Alloc(pCS_CHAT_RES_MESSAGE, pPlayer->sessionId_, pPlayer->sectorX_, pPlayer->sectorY_);
	g_jobQ.Enqueue((JOB*)pJob);
}

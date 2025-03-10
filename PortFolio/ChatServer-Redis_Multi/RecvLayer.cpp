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

	// ���Ǹ� �����ǰ� �α��� ���ϴٰ� ���Ƿ� ���ų� ���� Ÿ�Ӿƿ��ð��� �ɸ� ���
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

		// ��Ʈ��ũ ���̺귯�� �������� Ÿ�Ӿƿ��� �����ϱ� ������ �ƹ��͵� ����
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
			// ������ �̰� �����Ҹ��� ���� ��� ����͸�Ŭ�� ���Ӿ��ϸ� ������������ �ƿ� ���Ͼ��������
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			__debugbreak();
		}
	}
}

void RecvLayer::CS_CHAT_REQ_LOGIN(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, char* pSessionKey, Player* pPlayer)
{
	// ���� ��ū ���𽺿��� ����� �о����
	client* pClient = GetRedisClient();
	const auto& temp = pClient->get(std::to_string(accountNo));
	pClient->sync_commit();
	const auto& ret = temp._Get_value();

	// ���𽺿� ����Ű�� ���ų�, ���𽺿� ����� ����Ű�� �ٸ��ٸ� �α��� ����
	if (ret.is_null() || memcmp(pSessionKey, ret.as_string().c_str(), Player::SESSION_KEY_LEN) != 0)
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// SessionTimeOut�� �������� ���̺귯���� �α��� ó��
	pGameServer_->SetLogin(pPlayer->sessionId_);

	// ������ ���� Block���ɼ� ����
	DuplicateLogin::PushPlayerToLoginMapAndDisconnectIfSameAccountLoggedIn(accountNo, pPlayer->sessionId_);

	// �÷��̾� �α��� ó�� �� �������� �ʱ�ȭ
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
	// Ŭ�� ��ȿ���� ���� ��ǥ�� ��û�޴ٸ� ���������
	if (IsNonValidSector(sectorX, sectorY))
	{
		pGameServer_->Disconnect(pPlayer->sessionId_);
		return;
	}

	// ��Ŷ �����
	Packet* pCS_CHAT_RES_SECTOR_MOVE = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_SECTOR_MOVE(accountNo, sectorX, sectorY, pCS_CHAT_RES_SECTOR_MOVE);

	// ������ǥ�� ���� ��ǥ�� SectorMove�� ���°�찡 �վ �ϴ� ����ó����
	// �״�� �������θ� ����Ʈ�� ���� ���Ǿ��̵� �մ� ��찡 ����
	if (sectorX == pPlayer->sectorX_ && sectorY == pPlayer->sectorY_)
	{
		pGameServer_->SendPacket(pPlayer->sessionId_, pCS_CHAT_RES_SECTOR_MOVE);
		return;
	}

	// ���� ���Ϳ� ADD Job�� �����Ѵ�
	SECTOR_ADD_JOB* pSectorAddJob = SECTOR_ADD_JOB::addJobPool_.Alloc(pCS_CHAT_RES_SECTOR_MOVE, pPlayer->sessionId_, sectorX, sectorY);
	g_jobQ.Enqueue((JOB*)(pSectorAddJob));

	// �̹� ��� �Ǿ��մٸ� removeJob ����
	if (pPlayer->bRegisterAtSector_)
	{
		SECTOR_REMOVE_JOB* pSectorRemoveJob = SECTOR_REMOVE_JOB::removeJobPool_.Alloc(pPlayer->sessionId_, pPlayer->sectorX_, pPlayer->sectorY_);
		g_jobQ.Enqueue((JOB*)(pSectorRemoveJob));
	}
	else
	{
		pPlayer->bRegisterAtSector_ = true;
	}

	// �÷��̾� ������ǥ ���� -> �������� ���̻� ������� ������ �������� REQ_SECTOR_MOVE ���Ž� ���� ������ǥ�� ����Ѵ�
	pPlayer->sectorX_ = sectorX;
	pPlayer->sectorY_ = sectorY;
}

void RecvLayer::CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, WCHAR* pMessage, Player* pPlayer)
{
	Packet* pCS_CHAT_RES_MESSAGE = PACKET_ALLOC(Net);
	MAKE_CS_CHAT_RES_MESSAGE(accountNo, pPlayer->ID_, pPlayer->nickName_, messageLen, pMessage, pCS_CHAT_RES_MESSAGE);

	// �̸� ���ڵ� �ϰ� �������� SendPacket_ALREADY_ENCODED ��� -> �׷��� ������ ���� �����尡 �ϳ��� ����ȭ���۸� ���ڵ��Ϸ��ؼ� ���� �߻�
	pCS_CHAT_RES_MESSAGE->SetHeader<Net>();
	BROADCAST_JOB* pJob = BROADCAST_JOB::broadCastJobPool_.Alloc(pCS_CHAT_RES_MESSAGE, pPlayer->sessionId_, pPlayer->sectorX_, pPlayer->sectorY_);
	g_jobQ.Enqueue((JOB*)pJob);
}

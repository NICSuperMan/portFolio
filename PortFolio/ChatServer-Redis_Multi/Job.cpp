#include <WinSock2.h>
#include "Job.h"
#include "Sector.h"
extern LoginChatServer* g_pChatServer;

BROADCAST_JOB::BROADCAST_JOB(Packet* pPacket, ULONGLONG sessionId, WORD sectorX, WORD sectorY)
	:JOB{ sessionId }, pPacket_{ pPacket }, sectorX_{ sectorX }, sectorY_{ sectorY }, belowThan4WhenOneOrder_{ 4 }, refCnt_{ 0 }
{
	//��ε� ĳ���� ���ù����� �ϳ��� ��и鿡 ��� ���� ���
	if (!((sectorX > 23 && sectorX < 26) || (sectorY > 23 && sectorY < 26)))
	{
		int order = GetOrder(sectorX, sectorY);
		orderArr_[order] = true;
		belowThan4WhenOneOrder_ = order;
		InterlockedIncrement(&refCnt_);
		pPacket->IncreaseRefCnt();
	}
	else
	{
		// ���ñ��� ���� ��и鿡 ��ġ�� ���(�ִ� 4���� ��ħ)
		SECTOR_AROUND* pSectorAround = &g_sectorAround[sectorY][sectorX];
		bool bQuadrantArr[4] = { false };
		int numberOfDifferentOrder = 0;

		for (int i = 0; i < pSectorAround->sectorCount; ++i)
		{
			// ���� ��ǥ�� �ش��ϴ� ��и��� ���Ѵ�
			int order = GetOrder(pSectorAround->Around[i].sectorX, pSectorAround->Around[i].sectorY);

			// ���� ��и��� orderArr�� �����Ѵ�
			if (!bQuadrantArr[order])
			{
				bQuadrantArr[order] = true;
				orderArr_[order] = true;
				++numberOfDifferentOrder;
			}
		}
		InterlockedAdd(&pPacket->refCnt_, numberOfDifferentOrder);
		InterlockedAdd(&refCnt_, numberOfDifferentOrder);
	}
}

// ���ν����尡 ����
void BROADCAST_JOB::distributeJobToOrderQ()
{
	//��ε� ĳ���� ���ù����� �ϳ��� ��и鿡 ��� ���� ���
	if (belowThan4WhenOneOrder_ < 4)
	{
		orderJobQ_[belowThan4WhenOneOrder_].push(this);
		return;
	}

	// ���ñ��� ���� ��и鿡 ��ġ�� ���(�ִ� 4���� ��ħ) -> ���� 4�и� ť�� ��ť
	for (int i = 0; i < 4; ++i)
	{
		if (orderArr_[i])
		{
			orderJobQ_[i].push(this);
		}
	}
}

// PQCS ��Ŀ�����尡 ����
void BROADCAST_JOB::Excute(int order)
{
	//���� �������� ��� ���Ͱ� �ϳ��� ��и� �ȿ� �����ϴ� ���
	if (belowThan4WhenOneOrder_ < 4)
	{
		SendPacket_AROUND(&g_sectorAround[sectorY_][sectorX_], pPacket_);
	}
	else
	{
		std::pair<WORD, WORD> pPosition[9];
		int posNum = determineBroadCastPosition(pPosition, &g_sectorAround[sectorY_][sectorX_], order);
		SendPacket_Sector_Multiple(pPosition, posNum, pPacket_);
	}

	if (pPacket_->DecrementRefCnt() == 0)
	{
		PACKET_FREE(pPacket_);
	}

	// ���̻� this ���� ����
	if (InterlockedDecrement(&refCnt_) == 0)
	{
		broadCastJobPool_.Free(this);
	}
}

// PQCS ��Ŀ�����尡 ����
void JOB::FlushOrderJobQ(int order)
{
	//printf("order : %d\n", order);
	JOB* pJob;
	while (!orderJobQ_[order].empty())
	{
		pJob = orderJobQ_[order].front();
		orderJobQ_[order].pop();
		pJob->Excute(order);
	}

	if (InterlockedDecrement(&g_pChatServer->updateThreadWakeCount_) == 0)
	{
		SetEvent(g_pChatServer->hUpdateThreadWakeEvent_);
	}
}

SECTOR_ADD_JOB::SECTOR_ADD_JOB(Packet* pPacket, ULONGLONG sessionId, WORD addSectorX, WORD addSectorY)
	:JOB{ sessionId }, pPacket_{ pPacket }, order_{ GetOrder(addSectorX, addSectorY) }, addSectorX_{ addSectorX }, addSectorY_{ addSectorY }
{
}

void SECTOR_ADD_JOB::distributeJobToOrderQ()
{
	orderJobQ_[order_].push(this);
}

void SECTOR_ADD_JOB::Excute(int order)
{
	RegisterClientAtSector(addSectorX_, addSectorY_, sessionId_);
	g_pChatServer->SendPacket(sessionId_, pPacket_);

	// ���̻� this ���� ����
	addJobPool_.Free(this);
}

SECTOR_REMOVE_JOB::SECTOR_REMOVE_JOB(ULONGLONG sessionId, WORD removeSectorX, WORD removeSectorY)
	:JOB{ sessionId }, order_{ GetOrder(removeSectorX,removeSectorY) }, removeSectorX_{ removeSectorX }, removeSectorY_{ removeSectorY }
{
}

void SECTOR_REMOVE_JOB::distributeJobToOrderQ()
{
	orderJobQ_[order_].push(this);
}

void SECTOR_REMOVE_JOB::Excute(int order)
{
	RemoveClientAtSector(removeSectorX_, removeSectorY_, sessionId_);

	// ���̻� this ���� ����
	removeJobPool_.Free(this);
}


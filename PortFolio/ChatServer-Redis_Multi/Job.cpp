#include <WinSock2.h>
#include "Job.h"
#include "Sector.h"
extern LoginChatServer* g_pChatServer;

BROADCAST_JOB::BROADCAST_JOB(Packet* pPacket, ULONGLONG sessionId, WORD sectorX, WORD sectorY)
	:JOB{ sessionId }, pPacket_{ pPacket }, sectorX_{ sectorX }, sectorY_{ sectorY }, belowThan4WhenOneOrder_{ 4 }, refCnt_{ 0 }
{
	//브로드 캐스팅 가시범위가 하나의 사분면에 모두 들어가는 경우
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
		// 가시권이 여러 사분면에 겹치는 경우(최대 4개에 걸침)
		SECTOR_AROUND* pSectorAround = &g_sectorAround[sectorY][sectorX];
		bool bQuadrantArr[4] = { false };
		int numberOfDifferentOrder = 0;

		for (int i = 0; i < pSectorAround->sectorCount; ++i)
		{
			// 섹터 좌표에 해당하는 사분면을 구한다
			int order = GetOrder(pSectorAround->Around[i].sectorX, pSectorAround->Around[i].sectorY);

			// 구한 사분면을 orderArr에 저장한다
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

// 메인스레드가 실행
void BROADCAST_JOB::distributeJobToOrderQ()
{
	//브로드 캐스팅 가시범위가 하나의 사분면에 모두 들어가는 경우
	if (belowThan4WhenOneOrder_ < 4)
	{
		orderJobQ_[belowThan4WhenOneOrder_].push(this);
		return;
	}

	// 가시권이 여러 사분면에 겹치는 경우(최대 4개에 걸침) -> 각각 4분면 큐에 인큐
	for (int i = 0; i < 4; ++i)
	{
		if (orderArr_[i])
		{
			orderJobQ_[i].push(this);
		}
	}
}

// PQCS 워커스레드가 실행
void BROADCAST_JOB::Excute(int order)
{
	//가시 범위안의 모든 섹터가 하나의 사분면 안에 존재하는 경우
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

	// 더이상 this 참조 금지
	if (InterlockedDecrement(&refCnt_) == 0)
	{
		broadCastJobPool_.Free(this);
	}
}

// PQCS 워커스레드가 실행
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

	// 더이상 this 참조 금지
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

	// 더이상 this 참조 금지
	removeJobPool_.Free(this);
}


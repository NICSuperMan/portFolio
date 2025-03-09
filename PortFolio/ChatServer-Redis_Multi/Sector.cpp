#include <WinSock2.h>
#include <list>
#include <emmintrin.h>
#include <utility>
#include "LoginChatServer.h"
#include "Sector.h"
extern LoginChatServer* g_pChatServer;

std::list<ULONGLONG> listArr[NUM_OF_SECTOR_VERTICAL][NUM_OF_SECTOR_HORIZONTAL];
int determineBroadCastPosition(std::pair<WORD, WORD>* pOutPosArr, SECTOR_AROUND* pSectorAround, int order)
{
	int len = 0;
	WORD quadrantY = order / 2;
	WORD quadrantX = order % 2;

	// 섹터 좌표가 포함되는 사분면 == order매개변수 인경우에 pOutPostArr에 섹터좌표를 저장한다
	for (int i = 0; i < pSectorAround->sectorCount; ++i)
	{
		if (pSectorAround->Around[i].sectorY / SECTOR_DENOMINATOR == quadrantY && pSectorAround->Around[i].sectorX / SECTOR_DENOMINATOR == quadrantX)
		{
			pOutPosArr[len] = { pSectorAround->Around[i].sectorY,pSectorAround->Around[i].sectorX };
			++len;
		}
	}

	return len;
}

void GetSectorAround(SHORT sectorX, SHORT sectorY, SECTOR_AROUND* pOutSectorAround)
{
	pOutSectorAround->sectorCount = 0;
	__m128i posY = _mm_set1_epi16(sectorY);
	__m128i posX = _mm_set1_epi16(sectorX);

	// 8방향 방향벡터 X성분 Y성분 준비
	__m128i DirVX = _mm_set_epi16(+1, +0, -1, +1, -1, +1, +0, -1);
	__m128i DirVY = _mm_set_epi16(+1, +1, +1, +0, +0, -1, -1, -1);

	// 더한다
	posX = _mm_add_epi16(posX, DirVX);
	posY = _mm_add_epi16(posY, DirVY);

	__m128i min = _mm_set1_epi16(-1);
	__m128i max = _mm_set1_epi16(NUM_OF_SECTOR_VERTICAL);

	// PosX > min ? 0xFFFF : 0
	__m128i cmpMin = _mm_cmpgt_epi16(posX, min);
	// PosX < max ? 0xFFFF : 0
	__m128i cmpMax = _mm_cmplt_epi16(posX, max);
	__m128i resultX = _mm_and_si128(cmpMin, cmpMax);

	SHORT X[8];
	_mm_storeu_si128((__m128i*)X, posX);

	SHORT Y[8];
	cmpMin = _mm_cmpgt_epi16(posY, min);
	cmpMax = _mm_cmplt_epi16(posY, max);
	__m128i resultY = _mm_and_si128(cmpMin, cmpMax);
	_mm_storeu_si128((__m128i*)Y, posY);

	// _mm128i min은 더이상 쓸일이 없으므로 재활용한다.
	min = _mm_and_si128(resultX, resultY);

	SHORT ret[8];
	_mm_storeu_si128((__m128i*)ret, min);

	BYTE sectorCount = 0;
	for (int i = 0; i < 4; ++i)
	{
		if (ret[i] == (short)0xffff)
		{
			pOutSectorAround->Around[sectorCount].sectorY = Y[i];
			pOutSectorAround->Around[sectorCount].sectorX = X[i];
			++sectorCount;
		}
	}

	pOutSectorAround->Around[sectorCount].sectorY = sectorY;
	pOutSectorAround->Around[sectorCount].sectorX = sectorX;
	++sectorCount;

	for (int i = 4; i < 8; ++i)
	{
		if (ret[i] == (short)0xffff)
		{
			pOutSectorAround->Around[sectorCount].sectorY = Y[i];
			pOutSectorAround->Around[sectorCount].sectorX = X[i];
			++sectorCount;
		}
	}
	pOutSectorAround->sectorCount = sectorCount;
}

void RegisterClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sessionId)
{
	listArr[sectorY][sectorX].push_back(sessionId);
}

void RemoveClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sessionId)
{
	listArr[sectorY][sectorX].remove(sessionId);
}

void SendPacket_Sector_Multiple(std::pair<WORD, WORD>* pPosArr, int len, Packet* pPacket)
{
	for (int i = 0; i < len; ++i)
	{
		for (ULONGLONG otherSessionId : listArr[pPosArr[i].first][pPosArr[i].second])
		{
			g_pChatServer->SendPacket_ALREADY_ENCODED(otherSessionId, pPacket);
		}
	}
}

void SendPacket_AROUND(SECTOR_AROUND* pSectorAround, Packet* pPacket)
{
	for (int i = 0; i < pSectorAround->sectorCount; ++i)
	{
		for (ULONGLONG otherSessionId : listArr[pSectorAround->Around[i].sectorY][pSectorAround->Around[i].sectorX])
		{
			g_pChatServer->SendPacket_ALREADY_ENCODED(otherSessionId, pPacket);
		}
	}
}

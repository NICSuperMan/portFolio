#pragma once
static constexpr auto NUM_OF_SECTOR_VERTICAL = 50;
static constexpr auto NUM_OF_SECTOR_HORIZONTAL = 50;
static constexpr auto SECTOR_DENOMINATOR = NUM_OF_SECTOR_VERTICAL / 2;


struct SECTOR_AROUND
{
	struct
	{
		WORD sectorX;
		WORD sectorY;
	}Around[9];
	BYTE sectorCount;
};

// Baking
inline SECTOR_AROUND g_sectorAround[NUM_OF_SECTOR_VERTICAL][NUM_OF_SECTOR_HORIZONTAL];

class GameServer;
class Packet;

int determineBroadCastPosition(std::pair<WORD, WORD>* pOutPosArr, SECTOR_AROUND* pSectorAround, int order);
void GetSectorAround(SHORT sectorX, SHORT sectorY, SECTOR_AROUND* pOutSectorAround);
void RegisterClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sessionId);
void RemoveClientAtSector(WORD sectorX, WORD sectorY, ULONGLONG sessionId);
void SendPacket_Sector_Multiple(std::pair<WORD, WORD>* pPosArr, int len, Packet* pPacket);
void SendPacket_AROUND(SECTOR_AROUND* pSectorAround, Packet* pPacket);

__forceinline bool IsNonValidSector(WORD sectorX, WORD sectorY)
{
	return !((0 <= sectorX) && (sectorX <= NUM_OF_SECTOR_VERTICAL)) && ((0 <= sectorY) && (sectorY <= NUM_OF_SECTOR_HORIZONTAL));
}

// 플레이어의 좌표를 가지고 사분면을 구하는 함수
// SECTOR_DENOMINATOR == 섹터 가로(세로)의 절반크기
__forceinline BYTE GetOrder(WORD sectorX, WORD sectorY)
{
	return (sectorY / SECTOR_DENOMINATOR) * 2 + (sectorX / SECTOR_DENOMINATOR);
}

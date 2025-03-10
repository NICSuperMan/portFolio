#pragma once
#include "Player.h"

static constexpr auto NUM_OF_SECTOR_VERTICAL = 50;
static constexpr auto NUM_OF_SECTOR_HORIZONTAL = 50;


struct SECTOR_AROUND
{
	struct
	{
		WORD sectorX;
		WORD sectorY;
	}Around[9];
	BYTE sectorCount;
};

struct SectorArray
{
	CLinkedList(*listArr)[NUM_OF_SECTOR_VERTICAL];
	SectorArray()
	{
		listArr = (CLinkedList(*)[NUM_OF_SECTOR_VERTICAL])malloc(sizeof(CLinkedList) * NUM_OF_SECTOR_HORIZONTAL * NUM_OF_SECTOR_VERTICAL);
		for (int y = 0; y < NUM_OF_SECTOR_VERTICAL; ++y)
		{
			for (int x = 0; x < NUM_OF_SECTOR_HORIZONTAL; ++x)
			{
				new(&listArr[y][x])CLinkedList{ offsetof(Player,sectorLink) };
			}
		}
	}
};

void GetSectorAround(SHORT sectorX, SHORT sectorY, SECTOR_AROUND* pOutSectorAround);
void SendPacket_AROUND(SECTOR_AROUND* pSectorAround, SmartPacket& sp);
void RegisterClientAtSector(WORD sectorX, WORD sectorY, Player* pPlayer);
void RemoveClientAtSector(WORD sectorX, WORD sectorY, Player* pPlayer);
void GetSectorMonitoringInfo(Packet* pOutPacket);
//void DebugForSectorProb(Player* pPlayer);

__forceinline bool IsNonValidSector(WORD sectorX, WORD sectorY)
{
	return !(((0 <= sectorX) && (sectorX <= NUM_OF_SECTOR_VERTICAL)) && ((0 <= sectorY) && (sectorY <= NUM_OF_SECTOR_HORIZONTAL)));
}

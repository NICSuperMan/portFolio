#pragma once
#include<windows.h>
#include "CLinkedList.h"

struct Player
{
	static constexpr int ID_LEN = 20;
	static constexpr int NICK_NAME_LEN = 20;
	static constexpr int SESSION_KEY_LEN = 64;
	static inline int MAX_PLAYER_NUM;
	static inline Player* pPlayerArr;
	static constexpr int INITIAL_SECTOR_VALUE = 51;

	static inline CRITICAL_SECTION MonitorSectorInfoCs;
	static inline ULONGLONG MonitoringClientSessionID = MAXULONGLONG;

	bool bLogin_;
	bool bRegisterAtSector_;
	bool bMonitoringLogin_;
	WORD sectorX_;
	WORD sectorY_;
	LINKED_NODE contentLink;
	LINKED_NODE sectorLink;
	ULONGLONG sessionId_;
	INT64 accountNo_;
	WCHAR ID_[ID_LEN];
	WCHAR nickName_[NICK_NAME_LEN];

	Player()
		:
		sectorLink{ offsetof(Player,sectorLink) },
		contentLink{ offsetof(Player,contentLink) }, bLogin_{ false }, bRegisterAtSector_{ false }
	{}

	__forceinline void LoginInit()
	{
		bLogin_ = true;
		bRegisterAtSector_ = false;
	}

	static WORD MAKE_PLAYER_INDEX(ULONGLONG sessionId)
	{
		return sessionId & 0xFFFF;
	}
};

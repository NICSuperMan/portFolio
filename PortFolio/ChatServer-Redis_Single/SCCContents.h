#pragma once
#include "Packet.h"
#include "CommonProtocol.h"
void MAKE_CS_CHAT_RES_LOGIN(BYTE status, INT64 accountNo, SmartPacket& sp);
void MAKE_CS_CHAT_RES_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, SmartPacket& sp);
void MAKE_CS_CHAT_RES_MESSAGE(INT64 accountNo, WCHAR* pID, WCHAR* pNickName, WORD messageLen, WCHAR* pMessage, SmartPacket& sp);

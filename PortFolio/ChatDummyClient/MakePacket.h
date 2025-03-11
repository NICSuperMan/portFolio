#pragma once
#include <windows.h>

class SmartPacket;
void MAKE_CS_LOGIN_REQ_LOGIN(INT64 accountNo, const char* pSessionKey, SmartPacket& sp);
void MAKE_CS_CHAT_REQ_LOGIN(INT64 accountNo, const WCHAR* pID, const WCHAR* pNick, const char* pSessionKey, SmartPacket& sp);
void MAKE_CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, SmartPacket& sp);
void MAKE_CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, const WCHAR* pMessage, SmartPacket& sp);

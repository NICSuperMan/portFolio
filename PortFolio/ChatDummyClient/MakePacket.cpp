#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <Common/CommonProtocol.h>
#include "player.h"

void MAKE_CS_LOGIN_REQ_LOGIN(INT64 accountNo, const char* pSessionKey, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_LOGIN_REQ_LOGIN << accountNo;
	sp->PutData((char*)pSessionKey, Player::SESSION_KEY_LEN);
}

void MAKE_CS_CHAT_REQ_LOGIN(INT64 accountNo, const WCHAR* pID, const WCHAR* pNick, const char* pSessionKey, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_CHAT_REQ_LOGIN << accountNo;
	sp->PutData((char*)pID, sizeof(WCHAR) * Player::ID_LEN);
	sp->PutData((char*)pNick, sizeof(WCHAR) * Player::NICK_NAME_LEN);
	sp->PutData((char*)pSessionKey, Player::SESSION_KEY_LEN);
}

void MAKE_CS_CHAT_REQ_SECTOR_MOVE(INT64 accountNo, WORD sectorX, WORD sectorY, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_CHAT_REQ_SECTOR_MOVE << accountNo << sectorX << sectorY;
}

void MAKE_CS_CHAT_REQ_MESSAGE(INT64 accountNo, WORD messageLen, const WCHAR* pMessage, SmartPacket& sp)
{
	*sp << (WORD)en_PACKET_CS_CHAT_REQ_MESSAGE << accountNo << (WORD)(messageLen * sizeof(WCHAR));
	sp->PutData((char*)pMessage, sizeof(WCHAR) * messageLen);
}

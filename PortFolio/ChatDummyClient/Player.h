#pragma once
#include <windows.h>
#include "CurrentServerType.h"
#include "State.h"
#include "AccountInfo.h"

// testField
constexpr BYTE DUPLICATE_TARGET			= 0x80;
constexpr BYTE SESSION_TIMEOUT_TARGET	= 0x40;
constexpr BYTE LOGIN_TIMOUT_TARGET		= 0x20;
constexpr BYTE DISCONNECT_TARGET		= 0x10;
constexpr BYTE LOGIN_ACTIVATE_AT_LOGIN	= 0x08;
constexpr BYTE LOGIN_ACTIVATE_AT_CHAT	= 0x04;
constexpr BYTE REGISTER_AT_SECTOR		= 0x02;
constexpr BYTE RESERVE_SEND_MESSAGE		= 0x01;
constexpr BYTE INITIAL_FIELD			= 0x00;

__forceinline bool IS_VALID_FIELD_FOR_TEST(BYTE testField)
{
	// 이중로그인, ATTACK, TIMEOUT등의 각종 테스트와 DISCONNECT예정 등에 대해 아무것에도 해당이 안되야 함
	static constexpr BYTE MASK = (DUPLICATE_TARGET | SESSION_TIMEOUT_TARGET | LOGIN_TIMOUT_TARGET | DISCONNECT_TARGET);
	return (testField & MASK) == 0;
}


__forceinline bool IS_NEED_TO_STOP_SEND_MESSAGE(BYTE testField)
{
	static constexpr BYTE MASK = (DUPLICATE_TARGET | LOGIN_TIMOUT_TARGET);
	return (testField & MASK) == 0;
}

#define IS_PLAYER_DUPLICATE_TARGET			(num) ((num) & (DUPLICATE_TARGET) == (DUPLICATE_TARGET))
#define IS_PLAYER_ATTACK_TARGET				(num) ((num) & (SESSION_TIMEOUT_TARGET) == (SESSION_TIMEOUT_TARGET))
#define IS_PLAYER_TIMEOUT_TARGET(num)			(((num) & (LOGIN_TIMOUT_TARGET)) == (LOGIN_TIMOUT_TARGET))
#define IS_PLAYER_CALLED_DISCONNECT(num)		(((num) & (DISCONNECT_TARGET)) == (DISCONNECT_TARGET))
#define IS_PLAYER_AUTH_AT_LOGIN_SERVER(num)		(((num) & (LOGIN_ACTIVATE_AT_LOGIN)) == (LOGIN_ACTIVATE_AT_LOGIN))
#define IS_PLAYER_AUTH_AT_CHAT_SERVER(num)		(((num) & (LOGIN_ACTIVATE_AT_CHAT)) == (LOGIN_ACTIVATE_AT_CHAT))
#define IS_PLAYER_REGISTER_AT_SECTROR(num)		(((num) & (REGISTER_AT_SECTOR)) == (REGISTER_AT_SECTOR))
#define IS_PLAYER_SEND_MESSAGE_IS_RESERVED(num) (((num) & (RESERVE_SEND_MESSAGE)) == (RESERVE_SEND_MESSAGE))

struct LyRics;

struct Player
{
	static constexpr int ID_LEN = 20;
	static constexpr int NICK_NAME_LEN = 20;
	static constexpr int SESSION_KEY_LEN = 64;
	static inline int MAX_PLAYER_NUM = 5000;
	static inline Player* pChatPlayerArr;

	SERVERTYPE serverType_;
	BYTE field_;
	STATE state_;
	bool bDuplicateNew_; // 이중로그인 상황에서 새로접속하는 클라임을 나타내는 플래그
	ULONGLONG duplicateVictimSessionId_;
	ULONGLONG sessionId_;
	DWORD lastRecvTime_;
	DWORD lastSendTime_; // RTT 측정용
	DWORD lastHeartbeatSendtime_;
	WORD sectorX_;
	WORD sectorY_;
	const AccountInfo* pInfo_;
	const LyRics* pLastSentChat;
	SOCKADDR_IN sockAddr_;
};

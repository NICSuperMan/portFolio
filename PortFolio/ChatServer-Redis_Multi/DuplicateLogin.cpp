#include <WinSock2.h>
#include <Windows.h>
#include "DuplicateLogin.h"
#include "LoginChatServer.h"


extern LoginChatServer* g_pChatServer;

namespace DuplicateLogin
{
	CRITICAL_SECTION cs[10];
	std::unordered_map<INT64, ULONGLONG> loginPlayerMap[10];
}

void DuplicateLogin::Init()
{
	for (int i = 0; i < 10; ++i)
	{
		InitializeCriticalSection(cs + i);
	}
}

void DuplicateLogin::PushPlayerToLoginMapAndDisconnectIfSameAccountLoggedIn(INT64 accountNo, ULONGLONG sessionID)
{
	int idx = accountNo % 10;
	EnterCriticalSection(cs + idx);
	const auto& iter = loginPlayerMap[idx].find(accountNo);
	// 이미 같은 accountNo의 플레이어가 접속한 경우 이전에 접속한 플레이어를 내보낸다
	if (iter != loginPlayerMap[idx].end())
	{
		loginPlayerMap[idx].erase(iter);
		g_pChatServer->Disconnect(iter->second);
	}
	loginPlayerMap[idx].insert(std::make_pair(accountNo, sessionID));
	LeaveCriticalSection(cs + idx);
}

void DuplicateLogin::RemoveFromLoginPlayerMapWhenLogOut(INT64 accountNo, ULONGLONG sessionID)
{
	int idx = accountNo % 10;
	EnterCriticalSection(cs + idx);
	const auto& iter = loginPlayerMap[idx].find(accountNo);
	// 이중로그인으로 끊긴 클라가 아닌 경우
	if (iter->second == sessionID)
	{
		loginPlayerMap[idx].erase(iter);
	}
	LeaveCriticalSection(cs + idx);
}

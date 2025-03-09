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
	// �̹� ���� accountNo�� �÷��̾ ������ ��� ������ ������ �÷��̾ ��������
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
	// ���߷α������� ���� Ŭ�� �ƴ� ���
	if (iter->second == sessionID)
	{
		loginPlayerMap[idx].erase(iter);
	}
	LeaveCriticalSection(cs + idx);
}

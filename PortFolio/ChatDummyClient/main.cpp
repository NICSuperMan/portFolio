#include <WinSock2.h>
#include <windows.h>
#include "LoginDummy.h"
#include "ChatDummy.h"
#include "Update.h"
#include "Parser.h"
#pragma comment(lib,"TextParser.lib")

LoginDummy* g_pLoginDummy;
ChatDummy* g_pChatDummy;

int main()
{
	printf(
		"Select ID LIST\n\n"
		"Index 1 - ID1.txt\n"
		"Index 2 - ID2.txt\n"
		"Index 3 - ID3.txt\n"
		"Index 4 - ID4.txt\n\n"
		"Select Index Number... "
	);

	int idx;
	scanf_s("%d", &idx);
	WCHAR idFile[MAX_PATH];
	swprintf_s(idFile, MAX_PATH, L"ID%d.txt", idx);

	PARSER psr = CreateParser(L"DummyConfig.txt");
	WCHAR ip[16];
	GetValueWSTR(psr, ip, _countof(ip), L"LOGIN_SERVER_IP");
	g_pLoginDummy = new LoginDummy
	{ 
		TRUE,
		2,
		1000,
		TRUE,
		ip,
		(USHORT)GetValueINT(psr,L"LOGIN_SERVER_PORT"),
		GetValueUINT(psr,L"LOGIN_DUMMY_IOCP_WORKER_THREAD_NUMBER"),
		GetValueUINT(psr,L"LOGIN_DUMMY_IOCP_ACTIVE_THREAD_NUMBER"),
		GetValueINT(psr,L"CONNECT_CLIENT"),
		(BYTE)GetValueINT(psr,L"PACKET_CODE"),
		(BYTE)GetValueINT(psr,L"PACKET_KEY")
	};

	g_pChatDummy = new ChatDummy
	{
		TRUE,
		5,
		100,
		FALSE,
		nullptr,
		USHORT(),
		GetValueUINT(psr,L"CHAT_DUMMY_IOCP_WORKER_THREAD_NUMBER"),
		GetValueUINT(psr,L"CHAT_DUMMY_IOCP_ACTIVE_THREAD_NUMBER"),
		GetValueINT(psr,L"CONNECT_CLIENT"),
		(BYTE)GetValueINT(psr,L"PACKET_CODE"),
		(BYTE)GetValueINT(psr,L"PACKET_KEY")
	};

	int maxSession = GetValueINT(psr, L"CONNECT_CLIENT");
	int randConnect = GetValueINT(psr, L"RAND_CONNECT");
	int randDisconnect = GetValueINT(psr, L"RAND_DISCONNECT");
	int randContents = GetValueINT(psr, L"RAND_CONTENTS");
	int delayAction = GetValueINT(psr, L"DELAY_ACTION");
	int delayLogin = GetValueINT(psr, L"DELAY_LOGIN");

	ReleaseParser(psr);
	g_pLoginDummy->Start();
	g_pChatDummy->Start();

	UpdateLoop(maxSession, randConnect, randDisconnect, randContents, delayAction, delayLogin, idFile, L"ChatList.txt");
	return 0;
}
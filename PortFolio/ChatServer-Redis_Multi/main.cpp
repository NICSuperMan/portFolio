#include <Winsock2.h>
#include <Parser.h>
#include "LoginChatServer.h"

LoginChatServer* g_pChatServer;

int main()
{
	PARSER loginChatConfig = CreateParser(L"LoginChatConfig.txt");
	WCHAR ip[16];
	GetValueWSTR(loginChatConfig, ip, _countof(ip), L"BIND_IP");
	DWORD tickPerFrame = (BYTE)GetValueINT(loginChatConfig, L"TICK_PER_FRAME");
	LONG timeOutCheckInterval = GetValueINT(loginChatConfig, L"TIMEOUT_CHECK_INTERVAL");
	LONG sessionTimeOut = GetValueINT(loginChatConfig, L"SESSION_TIMEOUT");
	LONG authUserTimeOut = GetValueINT(loginChatConfig, L"AUTH_USER_TIMEOUT");

	g_pChatServer = new LoginChatServer{
	ip,
	(USHORT)GetValueINT(loginChatConfig, L"BIND_PORT"),
	GetValueUINT(loginChatConfig, L"IOCP_WORKER_THREAD"),
	GetValueUINT(loginChatConfig, L"IOCP_ACTIVE_THREAD"),
	GetValueINT(loginChatConfig, L"IS_ZERO_COPY"),
	GetValueINT(loginChatConfig, L"SESSION_MAX"),
	GetValueINT(loginChatConfig, L"USER_MAX"),
	(BYTE)GetValueINT(loginChatConfig, L"PACKET_CODE"),
	(BYTE)GetValueINT(loginChatConfig, L"PACKET_KEY")
	};

	ReleaseParser(loginChatConfig);
	g_pChatServer->Start(20, 1000, 3000, 40000);
	g_pChatServer->WaitUntilShutDown();
	return 0;
}
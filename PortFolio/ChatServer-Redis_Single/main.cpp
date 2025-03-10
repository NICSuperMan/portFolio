#include <winsock2.h>
#include "LoginChatServer.h"
#include <cpp_redis/cpp_redis>
#include "Parser.h"
#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")
#pragma comment (lib, "ws2_32.lib")

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

	PARSER loginChatLanConfig = CreateParser(L"LoginChatLanClientConfig.txt");
	GetValueWSTR(loginChatLanConfig, ip, _countof(ip), L"BIND_IP");
	CMClient* pChatLanClient = new CMClient{
		FALSE,
		100,
		150,
		ip,
		(USHORT)GetValueINT(loginChatLanConfig, L"BIND_PORT"),
		GetValueUINT(loginChatLanConfig, L"IOCP_WORKER_THREAD"),
		GetValueUINT(loginChatLanConfig, L"IOCP_ACTIVE_THREAD"),
		GetValueINT(loginChatLanConfig, L"IS_ZERO_COPY"),
		GetValueINT(loginChatLanConfig, L"SESSION_MAX"),
		SERVERNUM::CHAT 
	};
	ReleaseParser(loginChatLanConfig);

	// 서버 가동 시작
	g_pChatServer->RegisterMonitorLanClient(pChatLanClient);
	g_pChatServer->Start(tickPerFrame, timeOutCheckInterval, sessionTimeOut, authUserTimeOut);
	g_pChatServer->WaitUntilShutDown();
	delete g_pChatServer;
	return 0;
}
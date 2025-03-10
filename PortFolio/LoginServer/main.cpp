#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include "Parser.h"
#include "LoginServer.h"

LoginServer* g_pLoginServer;

int main()
{
	WCHAR ip[16];
	PARSER loginLanClientConfig = CreateParser(L"loginLanClientConfig.txt");
	GetValueWSTR(loginLanClientConfig, ip, _countof(ip), L"BIND_IP");
	CMClient* pLoginLanClient = new CMClient
	{
		FALSE,
		150,
		100,
		ip,
		(USHORT)GetValueINT(loginLanClientConfig, L"BIND_PORT"),
		GetValueUINT(loginLanClientConfig, L"IOCP_WORKER_THREAD"),
		GetValueUINT(loginLanClientConfig, L"IOCP_ACTIVE_THREAD"),
		(BOOL)GetValueINT(loginLanClientConfig, L"IS_ZERO_COPY"),
		GetValueINT(loginLanClientConfig, L"SESSION_MAX"),
		SERVERNUM::LOGIN
	};
	ReleaseParser(loginLanClientConfig);

	PARSER loginConfig = CreateParser(L"LoginServerConfig.txt");
	GetValueWSTR(loginConfig, ip, _countof(ip), L"BIND_IP");
	g_pLoginServer = new LoginServer
	(
		ip,
		(USHORT)GetValueINT(loginConfig, L"BIND_PORT"),
		GetValueUINT(loginConfig, L"IOCP_WORKER_THREAD"),
		GetValueUINT(loginConfig, L"IOCP_ACTIVE_THREAD"),
		(BOOL)GetValueINT(loginConfig, L"IS_ZERO_COPY"),
		GetValueINT(loginConfig, L"SESSION_MAX"),
		GetValueINT(loginConfig, L"USER_MAX"),
		(BYTE)GetValueUINT(loginConfig, L"PACKET_CODE"),
		(BYTE)GetValueUINT(loginConfig, L"PACKET_KEY"),
		(USHORT)GetValueUINT(loginConfig, L"CHATSERVER_PORT"),
		(BOOL)GetValueINT(loginConfig, L"bLoopBackTest"),
		pLoginLanClient
	);
	g_pLoginServer->Start();
	ReleaseParser(loginConfig);

	g_pLoginServer->WaitUntilShutDown();
	delete g_pLoginServer;
	return 0;
}


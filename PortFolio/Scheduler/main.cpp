#include <WinSock2.h>
#include <windows.h>
#include <Parser.h>
#include "MonitorLanServer.h"
#pragma comment(lib,"TextParser.lib")

int main()
{
	PARSER psr = CreateParser(L"MonitorNetConfig.txt");
	WCHAR ip[16];
	int len = GetValueWSTR(psr, ip, _countof(ip), L"BIND_IP");
	MonitorNetServer* pMonitorNetServer = new MonitorNetServer
	{ 
		ip,
		(const USHORT)GetValueINT(psr,L"BIND_PORT"),
		GetValueUINT(psr,L"IOCP_WORKER_THREAD"),
		GetValueUINT(psr,L"IOCP_ACTIVE_THREAD"),
		GetValueINT(psr,L"IS_ZERO_COPY"),
		GetValueINT(psr,L"SESSION_MAX"),
		GetValueINT(psr,L"USER_MAX"),
		(BYTE)GetValueINT(psr,L"PACKET_CODE"),
		(BYTE)GetValueINT(psr,L"PACKET_KEY"),
		FALSE,
		0,
		0 
	};
	ReleaseParser(psr);

	psr = CreateParser(L"MonitorLanConfig.txt");
	len = GetValueWSTR(psr, ip, _countof(ip), L"BIND_IP");
	MonitorLanServer* pMonitorLanServer = new MonitorLanServer
	{
		ip,
		(const USHORT)GetValueINT(psr,L"BIND_PORT"),
		GetValueUINT(psr,L"IOCP_WORKER_THREAD"),
		GetValueUINT(psr,L"IOCP_ACTIVE_THREAD"),
		GetValueINT(psr,L"IS_ZERO_COPY"),
		GetValueINT(psr,L"SESSION_MAX"),
		FALSE,
		1000,
		3000,
		GetValueUINT(psr,L"MONITOR_DATA_DB_WRITE_REQUEST_INTERVAL"),
		GetValueUINT(psr,L"DB_WRITE_TIMEOUT")
	};
	ReleaseParser(psr);

	pMonitorLanServer->Start(pMonitorNetServer);
	pMonitorLanServer->WaitUntilShutDown();

	return 0;
}
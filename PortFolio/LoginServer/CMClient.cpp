#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include "CommonProtocol.h"
#include "CMClient.h"


CMClient::CMClient(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, SERVERNUM WhatServerTypeOfLanClient)
	:LanClient{ bAutoReconnect,autoReconnectCnt,autoReconnectInterval,pIP,port,iocpWorkerNum,cunCurrentThreadNum,bZeroCopy,maxSession }, lanClientType_{ WhatServerTypeOfLanClient }
{
}

CMClient::~CMClient()
{
}

BOOL CMClient::Start()
{
	for (DWORD i = 0; i < IOCP_ACTIVE_THREAD_NUM_; ++i)
	{
		ResumeThread(hIOCPWorkerThreadArr_[i]);
	}
	Connect(false, &sockAddr_);
	return TRUE;
}

void CMClient::OnRecv(ULONGLONG id, Packet* pPacket)
{
	WORD Type;
	(*pPacket) >> Type;
	if (Type == 1)
	{
		InterlockedExchange((LONG*)&bLogin_, TRUE);
	}
	else
		__debugbreak();
}

void CMClient::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void CMClient::OnConnect(ULONGLONG id)
{
	LOG(L"ONOFF", SYSTEM, CONSOLE, L"MonitoringServer Connect Success");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MonitoringServer Connect Success");
	SmartPacket sp = PACKET_ALLOC(Lan);
	(*sp) << (WORD)en_PACKET_SS_MONITOR_LOGIN << (int)lanClientType_;
	SendPacket(id, sp);
}

void CALLBACK ReconnectTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired);

void CMClient::OnRelease(ULONGLONG id)
{
	InterlockedExchange((LONG*)&bLogin_, FALSE);

	// юс╫ц
	HANDLE hTimer;
	LanClientSession* pSession = pSessionArr_ + LanClientSession::GET_SESSION_INDEX(id);
	ReconnectQ_.Enqueue(pSession);
	CreateTimerQueueTimer(&hTimer, NULL, ReconnectTimer, (PVOID)((LanClient*)this), ((LanClient*)this)->autoReconnectInterval_, 0, WT_EXECUTEDEFAULT);
}

void CMClient::OnConnectFailed(ULONGLONG id)
{
}

void CMClient::OnAutoResetAllFailed()
{
}

void CMClient::SendToMonitoringServer(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp)
{
	if (!bLogin_)
		return;

	SmartPacket sp = PACKET_ALLOC(Lan);
	(*sp) << (WORD)en_PACKET_SS_MONITOR_DATA_UPDATE << serverNo << dataType << dataValue << timeStamp;
	SendPacket(pSessionArr_->id_, sp);
}

void CMClient::RequestLoginToMonitoringServer(ULONGLONG id, SERVERNUM serverNo)
{
	SmartPacket sp = PACKET_ALLOC(Lan);
	(*sp) << (WORD)en_PACKET_SS_MONITOR_LOGIN << (int)lanClientType_;
	SendPacket(id, sp);
}

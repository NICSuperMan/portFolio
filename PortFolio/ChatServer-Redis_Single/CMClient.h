#pragma once
#include "LanClient.h"
#include "CommonProtocol.h"
#include "ServerNum.h"

class CMClient : public LanClient
{
public:
	CMClient(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession,SERVERNUM WhatServerTypeOfLanClient);
	~CMClient();
	BOOL Start();
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnConnect(ULONGLONG id) override;
	virtual void OnRelease(ULONGLONG id) override;
	virtual void OnConnectFailed(ULONGLONG id) override;
	virtual void OnAutoResetAllFailed() override;
	void SendToMonitoringServer(BYTE serverNo, BYTE DataType, int dataValue, int timeStamp);
	void RequestLoginToMonitoringServer(ULONGLONG id, SERVERNUM serverNo);
	BOOL bLogin_ = FALSE;
	const SERVERNUM lanClientType_;
};

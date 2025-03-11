#pragma once
#include <NetClient/NetClient.h>
#include "ChatDummy.h"

class LoginDummy : public NetClient
{
public:
	LoginDummy(BOOL bAutoReconnect, LONG autoReconnectCnt, const LONG autoReconnectInterval, BOOL bUseMemberSockAddrIn, WCHAR* pIP, USHORT port, DWORD iocpWorkerThreadNum, DWORD cuncurrentThreadNum,
		LONG maxSession, BYTE packetCode, BYTE packetFixedKey);
	BOOL Start();
	~LoginDummy();
	virtual void OnRecv(ULONGLONG id, SmartPacket& sp) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnConnect(ULONGLONG id) override;
	virtual void OnRelease(ULONGLONG id) override;
	virtual void OnConnectFailed(ULONGLONG id) override;
	virtual void OnAutoResetAllFailed() override;
	int LoginConnectFailed_ = 0;
};

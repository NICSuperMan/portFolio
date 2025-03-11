#pragma once
#include <NetClient/NetClient.h>

class ChatDummy : public NetClient
{
public:
	ChatDummy(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, BOOL bUseMemberSockAddrIn, WCHAR* pIP, USHORT port, DWORD iocpWorkerThreadNum, DWORD cunCurrentThreadNum, LONG maxSession, BYTE packetCode, BYTE packetFixedKey);
	BOOL Start();
	virtual void OnRecv(ULONGLONG id, SmartPacket& sp) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnConnect(ULONGLONG id) override;
	virtual void OnRelease(ULONGLONG id) override;
	virtual void OnConnectFailed(ULONGLONG id) override;
	virtual void OnAutoResetAllFailed() override;
};

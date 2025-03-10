#pragma once
#include "GameServer.h"
#include "CMClient.h"
#include "HMonitor.h"
#include "ChatContents.h"
#include "LoginContents.h"
#include "GameServerTimeOut.h"

class LoginChatServer : public GameServer
{
public:
	LoginChatServer(WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, LONG maxUser, BYTE packetCode, BYTE packetfixedKey);
	~LoginChatServer();
	void Start(DWORD tickPerFrame, DWORD timeOutCheckInterval, LONG sessionTimeOut, LONG authUserTimeOut);
	void RegisterMonitorLanClient(CMClient* pClient);
	virtual BOOL OnConnectionRequest(const WCHAR* pIP, const USHORT port) override;
	virtual void* OnAccept(void* pPlayer) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnPost(void* order) override;
	virtual void OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown() override; 

	// Monitorable override
	virtual void OnMonitor() override; 
	alignas(64) LONG UPDATE_CNT_TPS = 0;
	alignas(64) ULONGLONG UPDATE_CNT_TOTAL = 0;
	alignas(64) ULONGLONG RECV_TOTAL = 0;
	static inline HMonitor monitor;
private:
	CMClient* pLanClient_ = nullptr;
	MonitoringUpdate* pConsoleMonitor_ = nullptr;
	LoginContents* pLogin_ = nullptr;
	ChatContents* pChatting_ = nullptr;
	GameServerTimeOut* pTimeOut_ = nullptr;
};

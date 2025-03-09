#pragma once
#include "GameServer.h"
#include "HMonitor.h"

class JobProcessLayer;
class RecvLayer;

class LoginChatServer : public GameServer
{
public:
	LoginChatServer(WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, LONG maxUser, BYTE packetCode, BYTE packetfixedKey);
	~LoginChatServer();
	void Start(DWORD tickPerFrame, DWORD timeOutCheckInterval, LONG sessionTimeOut, LONG authUserTimeOut);
	virtual BOOL OnConnectionRequest(const WCHAR* pIP, const USHORT port) override;
	virtual void* OnAccept(void* pPlayer) override;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) override;
	virtual void OnPost(void* order) override;
	virtual void OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown() override;
	virtual void OnMonitor() override;
	LONG updateThreadWakeCount_ = 4;
	HANDLE hUpdateThreadWakeEvent_;
	static inline HMonitor monitor;

private:
	MonitoringUpdate* pConsoleMonitor_ = nullptr;
	GameServerTimeOut* pTimeOut_ = nullptr;
	RecvLayer* pRecvLayer_ = nullptr;
	JobProcessLayer* pJobProcessLayer_ = nullptr;
	ULONGLONG PROCESS_CPU_TICK_ELAPSED_ = 0;
	ULONGLONG PROCESS_CPU_TICK_TIME_DIFF_ = 0;
	ULONGLONG RECV_TOTAL_ = 0;
};

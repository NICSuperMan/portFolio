#pragma once
#include <windows.h>

struct LanClientSession;

class SmartPacket;
class Packet;
struct LanClientSession;
#include "CLockFreeQueue.h"
#include "CLockFreeStack.h"
#include <MSWSock.h>

class LanClient
{
public:
	LanClient(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession);
	virtual ~LanClient();
	bool Connect(bool bRetry, SOCKADDR_IN* pSockAddrIn);
	void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	void SendPacket_ALREADY_ENCODED(ULONGLONG id, Packet* pPacket);
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	virtual void OnConnect(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnConnectFailed(ULONGLONG id) = 0;
	virtual void OnAutoResetAllFailed() = 0;
	void Disconnect(ULONGLONG id);
	void ShutDown(); //NetServer에서 바로 호출, 혹은 더미 메인스레드에서 바로 호출
protected:
	bool ConnectPost(bool bRetry, LanClientSession* pSession, SOCKADDR_IN* pSockAddrIn);
	BOOL SendPost(LanClientSession* pSession);
	BOOL RecvPost(LanClientSession* pSession);
	void ReleaseSession(LanClientSession* pSession);
	void RecvProc(LanClientSession* pSession, int numberOfBytesTransferred);
	void SendProc(LanClientSession* pSession, DWORD dwNumberOfBytesTransferred);
	void ConnectProc(LanClientSession* pSession);
	friend class Packet;
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
	static bool SetLinger(SOCKET sock);
	static bool SetZeroCopy(SOCKET sock);
	static bool SetReuseAddr(SOCKET sock);
	static bool SetClientBind(SOCKET sock);

	const DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	const DWORD IOCP_ACTIVE_THREAD_NUM_;
	LONG lSessionNum_ = 0;
	const LONG maxSession_;
	ULONGLONG ullIdCounter_ = 0;
	LanClientSession* pSessionArr_;
	HANDLE* hIOCPWorkerThreadArr_;
	CLockFreeStack<short> idxStack_;
	CLockFreeQueue<LanClientSession*> ReconnectQ_;
	HANDLE hcp_;
	LPFN_CONNECTEX lpfnConnectExPtr_;
	SOCKADDR_IN sockAddr_;

	const BOOL bAutoReconnect_;
	const LONG autoReconnectCnt_;
public:
	const LONG autoReconnectInterval_;
protected:
	friend void CALLBACK ReconnectTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired);
};

#include "Packet.h"
#include "RingBuffer.h"
#include "MYOVERLAPPED.h"
#include "LanClientSession.h"

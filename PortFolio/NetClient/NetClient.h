#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <windows.h>
#include <mutex>
#include <DataStructure/CLockFreeStack.h>
#include <DataStructure/CLockFreeQueue.h>

struct NetClientSession;

class SmartPacket;
class Packet;
struct NetClientSession;

class NetClient
{
public:
	NetClient(const BOOL bAutoReconnect, const LONG autoReconnectCnt, const LONG autoReconnectInterval, BOOL bUseMemberSockAddrIn, const WCHAR* pIP, const USHORT port, const DWORD iocpWorkerThreadNum, const DWORD cunCurrentThreadNum,
		const LONG maxSession, const BYTE packetCode, const BYTE packetFixedKey);
	virtual ~NetClient();
	void InitialConnect(SOCKADDR_IN* pSockAddrIn);
	bool Connect(bool bRetry, SOCKADDR_IN* pSockAddrIn);
	void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	virtual void OnRecv(ULONGLONG id, SmartPacket& sp) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	virtual void OnConnect(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnConnectFailed(ULONGLONG id) = 0;
	virtual void OnAutoResetAllFailed() = 0;
	void Disconnect(ULONGLONG id);
protected:
	bool ConnectPost(bool bRetry, NetClientSession* pSession, SOCKADDR_IN* pSockAddrIn);
	BOOL SendPost(NetClientSession* pSession);
	BOOL RecvPost(NetClientSession* pSession);
	void ReleaseSession(NetClientSession* pSession);
	void RecvProc(NetClientSession* pSession, int numberOfBytesTransferred);
	void SendProc(NetClientSession* pSession, DWORD dwNumberOfBytesTransferred);
	void ConnectProc(NetClientSession* pSession);
	void DisconnectProc(NetClientSession* pSession);
	friend class Packet;
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
	static bool SetLinger(SOCKET sock);
	static bool SetZeroCopy(SOCKET sock);
	static bool SetReuseAddr(SOCKET sock);
	static bool SetClientBind(SOCKET sock);

	const DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	const DWORD IOCP_ACTIVE_THREAD_NUM_ = 0;
	LONG lSessionNum_ = 0;
	const LONG maxSession_;
	ULONGLONG ullIdCounter_ = 0;
	NetClientSession* pSessionArr_;
	HANDLE* hIOCPWorkerThreadArr_;
	CLockFreeStack<short> DisconnectStack_;
	CLockFreeQueue<NetClientSession*> ReconnectQ_;
	HANDLE hcp_;
	LPFN_CONNECTEX lpfnConnectExPtr_;
	const BOOL bAutoReconnect_;
	const LONG autoReconnectCnt_;
	const LONG autoReconnectInterval_;
	friend void CALLBACK ReconnectTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired);
public:
	SOCKADDR_IN sockAddr_;
};

#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <SerialLizeBuffer_AND_RingBuffer/RingBuffer.h>
#include <Common/MYOVERLAPPED.h>
#include <NetClient/NetClientSession.h>

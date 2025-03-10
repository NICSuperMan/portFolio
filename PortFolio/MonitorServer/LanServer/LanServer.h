#pragma once
#include <Windows.h>
#include <DataStructure/CLockFreeStack.h>
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include "LanSession.h"

class LanServerTimeOut;
class SmartPacket;

class LanServer
{
public:
	LanServer(const WCHAR* pIP, const USHORT port, const DWORD IocpWorkerThreadNum, const DWORD CuncurrentThreadNum, const BOOL bZeroCopy, const LONG maxSession, const BOOL bActivateTimeOut, const LONG userTimeOut, const DWORD timeOutCheckInterval);
	void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	void SendPacket(ULONGLONG id, Packet* pPacket);
	virtual BOOL OnConnectionRequest(const WCHAR* pIP, const USHORT port) = 0;
	virtual void* OnAccept(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	virtual void OnPost(void* order) = 0;
	virtual void OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown() = 0; // 모든 워커스레드를 종료시키기전에 쏴야할일들을 정의하면됨, DB스레드에 대한 타임아웃 PQCS가 대표적
	virtual void OnResourceCleanAtShutDown() = 0;
	void Disconnect(ULONGLONG id);
	void WaitUntilShutDown(); // 메인스레드에서 서버켜고 셧다운까지 대기할떄 호출
	const WCHAR* GetSessionIP(ULONGLONG id);
	const USHORT GetSessionPort(ULONGLONG id);
protected:
	void ShutDown();
	void RequestShutDown(); // 워커스레드에서 서버를 종료시키고자 할때 호출
	const DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	const DWORD IOCP_ACTIVE_THREAD_NUM_ = 0;
	const LONG TIME_OUT_MILLISECONDS_ = 0;
	const ULONGLONG TIME_OUT_CHECK_INTERVAL_ = 0;
	ULONGLONG ullIdCounter = 0;
	LanSession* pSessionArr_;
	CLockFreeStack<short> DisconnectStack_;
	HANDLE hcp_;
	HANDLE hAcceptThread_;
	HANDLE* hIOCPWorkerThreadArr_;
	SOCKET hListenSock_;
	HANDLE hShutDownEvent_;
	LanServerTimeOut* pLanTimeOut_ = nullptr;

	static inline const MYOVERLAPPED OnPostOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::POST };

	BOOL RecvPost(LanSession* pSession);
	BOOL SendPost(LanSession* pSession);
	void ReleaseSession(LanSession* pSession);
	void RecvProc(LanSession* pSession, DWORD dwNumberOfBytesTransferred);
	void SendProc(LanSession* pSession, DWORD dwNumberOfBytesTransferred);

	static unsigned __stdcall AcceptThread(LPVOID arg);
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
public:
	alignas(64) ULONGLONG acceptTotal_ = 0;
	alignas(64) LONG64 recvTPS_ = 0;
	alignas(64) LONG64 sendTPS_ = 0;
	alignas(64) ULONGLONG disconnectTPS_ = 0;
	alignas(64) ULONGLONG acceptCounter_ = 0;
	alignas(64) LONG lSessionNum_ = 0;
protected:
	const LONG maxSession_ = 0;
	friend class LanServerTimeOut;
};

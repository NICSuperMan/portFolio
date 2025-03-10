#pragma once
#include <Common/MYOVERLAPPED.h>
#include <DataStructure/CLockFreeStack.h>


class Stack;
class SmartPacket;
struct NetSession;

class NetServer
{
public:
	NetServer(const WCHAR* pIP, const USHORT port, const DWORD IocpWorkerThreadNum, const DWORD CuncurrentThreadNum,
		const BOOL bZeroCopy, const LONG maxSession, const BYTE packetCode, const BYTE packetFixedKey, const BOOL bActivateTimeOut, const LONG userTimeOut, const DWORD timeOutCheckInterval);
	NetServer(const WCHAR* pTextFileStr);
	// Test��
	void SendPacket(ULONGLONG id, SmartPacket& sendPacket);
	void SendPacket(ULONGLONG id, Packet* pPacket);
	void SendPacket_ALREADY_ENCODED(ULONGLONG id, Packet* pPacket);
	void SendPacket_ENQUEUE_ONLY(ULONGLONG id, Packet* pPacket);
	virtual BOOL OnConnectionRequest(const WCHAR* pIP, USHORT port) = 0;
	virtual void* OnAccept(ULONGLONG id) = 0;
	virtual void OnRelease(ULONGLONG id) = 0;
	virtual void OnRecv(ULONGLONG id, Packet* pPacket) = 0;
	virtual void OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket) = 0;
	virtual void OnPost(void* order) = 0;
	virtual void OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown() = 0; // �Ϲ������� DB�����忡���� PQCS�� �� ����Ұ��̴�
	virtual void OnResourceCleanAtShutDown() = 0;

	// ShutDown �Ҷ� ���°� �Լ�
	void WaitUntilShutDown(); // ���ν������ start���� ������ �̰� ȣ���ؾ��Ѵ�
	void ShutDown(); // WaitUntilShutDown���� ȣ��
	void RequestShutDown(); // ��Ŀ�����尡 ȣ��
	// ������
	void Disconnect(ULONGLONG id);
	const WCHAR* GetIP(ULONGLONG sessionId);
	const USHORT GetPort(ULONGLONG sessionId);

private:
	void ProcessTimeOut();
	static unsigned __stdcall AcceptThread(LPVOID arg);
	static unsigned __stdcall IOCPWorkerThread(LPVOID arg);
public:
	// Accept
	DWORD IOCP_WORKER_THREAD_NUM_ = 0;
	DWORD IOCP_ACTIVE_THREAD_NUM_ = 0;
	LONG lSessionNum_ = 0;
	LONG maxSession_ = 0;
	LONG TIME_OUT_MILLISECONDS_ = 0;
	ULONGLONG TIME_OUT_CHECK_INTERVAL_ = 0;
	ULONGLONG ullIdCounter = 0;
	NetSession* pSessionArr_;
	CLockFreeStack<short> DisconnectStack_;
	const MYOVERLAPPED SendWorkerOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::SEND_WORKER };
	const MYOVERLAPPED OnPostOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::POST };
	HANDLE hcp_;
	HANDLE hAcceptThread_;
	HANDLE* hIOCPWorkerThreadArr_;
	HANDLE hShutDownEvent_;
	SOCKET hListenSock_;
	LONG updateThreadSendCounter_ = 0;

	virtual BOOL RecvPost(NetSession* pSession);
	virtual BOOL SendPost(NetSession* pSession);
	virtual void ReleaseSession(NetSession* pSession);
	void RecvProc(NetSession* pSession, int numberOfBytesTransferred);
	void SendProc(NetSession* pSession, DWORD dwNumberOfBytesTransferred);
	friend class Packet;

	int bAccSend = 0;
	// Monitoring ����
	// Recv (Per MSG)

public:
	LONG lPlayerNum = 0;
	ULONGLONG acceptCounter_ = 0;
	alignas(64) ULONGLONG acceptTotal_ = 0;
	alignas(64) ULONGLONG recvTPS_ = 0;

	// Send (Per MSG)
	alignas(64) LONG sendTPS_ = 0;

	// Disconnect
	alignas(64) ULONGLONG disconnectTPS_ = 0;
};


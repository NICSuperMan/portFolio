#include <WinSock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#include <locale>
#include <process.h>
#include <Common/MyOverlapped.h>
#include <Assert.h>
#include "LanClient.h"
#include "Logger.h"
#include "Parser.h"
#include "LanClientSession.h"
#pragma comment(lib,"Winmm.lib")
#pragma comment(lib,"LoggerMT.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"TextParser.lib")

template<typename T>
__forceinline T& IGNORE_CONST(const T& value)
{
	return const_cast<T&>(value);
}

bool WsaIoctlWrapper(SOCKET sock, GUID guid, LPVOID* pFuncPtr)
{
	DWORD bytes = 0;
	return SOCKET_ERROR != WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), pFuncPtr, sizeof(*pFuncPtr), &bytes, NULL, NULL);
}

void CALLBACK ReconnectTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	static MYOVERLAPPED ReconnectOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::RECONNECT };

	LanClient* pLanClient = (LanClient*)lpParam;
#pragma warning(disable : 26815)
	LanClientSession* pSession = pLanClient->ReconnectQ_.Dequeue().value();
#pragma warning(default: 26815)
	PostQueuedCompletionStatus(pLanClient->hcp_, 2, (ULONG_PTR)pSession, &ReconnectOverlapped.overlapped);
}

LanClient::LanClient(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession)
	:bAutoReconnect_{ bAutoReconnect }, autoReconnectCnt_{ autoReconnectCnt }, autoReconnectInterval_{ autoReconnectInterval }, IOCP_WORKER_THREAD_NUM_{ iocpWorkerNum },IOCP_ACTIVE_THREAD_NUM_{cunCurrentThreadNum}, maxSession_{maxSession}
{
	timeBeginPeriod(1);
	WSADATA wsaData;
#pragma warning(disable : 6031)
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#pragma warning(default: 6031)

	// ConnectEx �Լ������� ��������
	SOCKET dummySock = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_LOG(WsaIoctlWrapper(dummySock, WSAID_CONNECTEX, (LPVOID*)&lpfnConnectExPtr_) == false,L"WSAIoCtl ConnectEx Failed");
	closesocket(dummySock);

	// sockAddr_ �ʱ�ȭ, ConnectEx���� ���
	// LanClient�� ������ Lan����̶� Bind�� IP�� ������ �ʱ⿡ ������
	ZeroMemory(&sockAddr_, sizeof(sockAddr_));
	sockAddr_.sin_family = AF_INET;
	InetPtonW(AF_INET, pIP, &sockAddr_.sin_addr);
	sockAddr_.sin_port = htons(port);

	// IOCP �ڵ� ����
	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, cunCurrentThreadNum);
	ASSERT_LOG(hcp_ == NULL, L"CreateIoCompletionPort Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	// ���� 17��Ʈ�� ������ ������Ʈ�� 16�� ���ϰ� �Ǵ³����� ������� ū�׸��̴�.
	ASSERT_FALSE_LOG(CAddressTranslator::CheckMetaCntBits(), L"LockFree 17bits Over");

	pSessionArr_ = new LanClientSession[maxSession_];
	for (int i = maxSession_ - 1; i >= 0; --i)
		idxStack_.Push(i);

	// IOCP ��Ŀ������ ����(CREATE_SUSPENDED)
	hIOCPWorkerThreadArr_ = new HANDLE[iocpWorkerNum];
	for (DWORD i = 0; i < iocpWorkerNum; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, CREATE_SUSPENDED, nullptr);
		ASSERT_ZERO_LOG(hIOCPWorkerThreadArr_[i], L"MAKE WorkerThread Fail");
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", iocpWorkerNum);
}

LanClient::~LanClient()
{
	WSACleanup();
	delete[] pSessionArr_;
	delete[] hIOCPWorkerThreadArr_;
}


bool LanClient::Connect(bool bRetry, SOCKADDR_IN* pSockAddrIn)
{
	auto opt = idxStack_.Pop();
	if (opt.has_value() == false)
		return false;

	LanClientSession* pSession = pSessionArr_ + opt.value();
	ConnectPost(bRetry, pSession, pSockAddrIn);
	return true;
}

void LanClient::SendPacket(ULONGLONG id, SmartPacket& sendPacket)
{
	LanClientSession* pSession = pSessionArr_ + LanClientSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// �̹� RELEASE �������̰ų� RELEASE�� ���
	if ((IoCnt & LanClientSession::RELEASE_FLAG) == LanClientSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE �Ϸ��� �ٽ� ���ǿ� ���� �ʱ�ȭ�� �Ϸ�Ȱ�� �� ��Ȱ��
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// ��� ����
	sendPacket->SetHeader<Lan>();
	sendPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(sendPacket.GetPacket());
	SendPost(pSession);
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanClient::SendPacket_ALREADY_ENCODED(ULONGLONG id, Packet* pPacket)
{
	LanClientSession* pSession = pSessionArr_ + LanClientSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// �̹� RELEASE �������̰ų� RELEASE�� ���
	if ((IoCnt & LanClientSession::RELEASE_FLAG) == LanClientSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE �Ϸ��� �ٽ� ���ǿ� ���� �ʱ�ȭ�� �Ϸ�Ȱ�� �� ��Ȱ��
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);
	SendPost(pSession);
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanClient::Disconnect(ULONGLONG id)
{
	LanClientSession* pSession = pSessionArr_ + LanClientSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// RELEASE������ Ȥ�� ����Ϸ�
	if ((IoCnt & LanClientSession::RELEASE_FLAG) == LanClientSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE�� ��Ȱ����� �Ǿ�����
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// Disconnect 1ȸ ����
	if (InterlockedExchange((LONG*)&pSession->bDisconnectCalled_, TRUE) == TRUE)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// ���� �����޴ٸ� ���� ���ǿ� ���ؼ� RELEASE ���� ȣ����� ������������ ����ȴ�
	CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);
	CancelIoEx((HANDLE)pSession->sock_, &pSession->recvOverlapped.overlapped);

	// CancelIoExȣ��� ���ؼ� RELEASE�� ȣ��Ǿ���� ������ �������� InterlockedIncrement ������ ȣ���� �ȵ� ��� ����û��
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanClient::ShutDown()
{
	// ��� Ŭ���̾�Ʈ�� �����κ��� ������������ �ݺ�
	while (idxStack_.num_ < maxSession_)
	{
		for (int i = 0; i < maxSession_; ++i)
		{
			CancelIoEx((HANDLE)pSessionArr_[i].sock_, &pSessionArr_[i].sendOverlapped.overlapped);
			CancelIoEx((HANDLE)pSessionArr_[i].sock_, &pSessionArr_[i].recvOverlapped.overlapped);
			InterlockedExchange((LONG*)&pSessionArr_[i].bDisconnectCalled_, TRUE);
		}
	}

	// ��Ŀ�����带 �����ϱ����� PQCS�� ��� ����Ѵ�
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		PostQueuedCompletionStatus(hcp_, 0, 0, 0);

	WaitForMultipleObjects(IOCP_WORKER_THREAD_NUM_, hIOCPWorkerThreadArr_, TRUE, INFINITE);

	// For Debug
	if (idxStack_.num_ != maxSession_)
		__debugbreak();

	for (int i = 0; i < maxSession_; ++i)
		idxStack_.Pop();

	CloseHandle(hcp_);
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		CloseHandle(hIOCPWorkerThreadArr_[i]);
}

#pragma warning(disable : 6387)
bool LanClient::ConnectPost(bool bRetry, LanClientSession* pSession, SOCKADDR_IN* pSockAddrIn)
{
	if (!bRetry)
	{
		memcpy(&pSession->sockAddrIn_, pSockAddrIn, sizeof(SOCKADDR_IN));
		InterlockedExchange(&pSession->reConnectCnt_, autoReconnectCnt_);
	}

	// ���� ����, �ɼ� ������ IOCP�� ���
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SetClientBind(sock);
	SetZeroCopy(sock);
	SetLinger(sock);
	if (NULL == CreateIoCompletionPort((HANDLE)sock, hcp_, (ULONG_PTR)pSession, 0))
	{
		DWORD errCode = WSAGetLastError();
		__debugbreak();
	}
	pSession->sock_ = sock;

	ZeroMemory(&pSession->connectOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	pSession->connectOverlapped.why = OVERLAPPED_REASON::CONNECT;
	BOOL bConnectExRet = lpfnConnectExPtr_(sock, (SOCKADDR*)&sockAddr_, sizeof(SOCKADDR_IN), nullptr, 0, NULL, &pSession->connectOverlapped.overlapped);
	if (bConnectExRet == FALSE)
	{
		DWORD errCode = WSAGetLastError();
		if (errCode == WSA_IO_PENDING)
			return true;

		closesocket(sock);
		OnConnectFailed(pSession->id_);
		if (bAutoReconnect_)
		{
			if (InterlockedDecrement(&pSession->reConnectCnt_) > 0)
			{
				HANDLE hTimer;
				if (0 == CreateTimerQueueTimer(&hTimer, NULL, ReconnectTimer, (PVOID)this, 100, 0, WT_EXECUTEDEFAULT))
				{
					DWORD errCode = GetLastError();
					__debugbreak();
				}
			}
		}
		else
			idxStack_.Push((short)(pSession - pSessionArr_));

		LOG(L"ERROR", ERR, TEXTFILE, L"ConnectEx ErrCode : %u", errCode);
		__debugbreak();
		return false;
	}
	return true;
}
#pragma warning(default : 6387)


BOOL LanClient::SendPost(LanClientSession* pSession)
{
	DWORD dwBufferNum;
	while (1)
	{
		if (pSession->sendPacketQ_.GetSize() <= 0)
			return FALSE;

		// ���� ���� TRUE�� �ٲ۴�. ���� TRUE���ٸ� ��ȯ���� TRUE�ϰ��̸� �׷��ٸ� ���� SEND �������̱� ������ �׳� ����������
		// �� ���ǹ��� ��ġ�� ���Ͽ� Out�� �ٲ��� ���������� ����ȴ�.
		// ������ SendPost ������ü�� Send�Ϸ����� �������� ��쿡�� in�� ��ġ�� SendPacket���� ���ؼ� �ٲ���� �ִ�.
		// iUseSize�� ���ϴ� ���������� DirectDequeueSize�� ���� �޶������ִ�.
		if (InterlockedExchange((LONG*)&pSession->bSendingInProgress_, TRUE) == TRUE)
			return TRUE;

		// SendPacket���� in�� �Űܼ� UseSize�� 0���� Ŀ���������� Send�Ϸ������� �����ؼ� Out�� �ű�� �÷��� ���� Recv�Ϸ����� �����尡 ���� SendPost�� ������ �÷��׸� �����Ѱ�� UseSize�� 0�̳��´�.
		// ���⼭ flag�� �ٽ� FALSE�� �ٲپ����� �ʾƼ� ����߻�
		dwBufferNum = pSession->sendPacketQ_.GetSize();

		if (dwBufferNum <= 0)
			InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
		else
			break;

	}

	WSABUF wsa[50];
	DWORD i;
	for (i = 0; i < 50 && i < dwBufferNum; ++i)
	{
#pragma warning(disable : 26815)
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
#pragma warning(default: 26815)
		wsa[i].buf = (char*)pPacket->pBuffer_;
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(Packet::LanHeader);
		pSession->pSendPacketArr_[i] = pPacket;
	}

	InterlockedExchange(&pSession->lSendBufNum_, i);
	InterlockedIncrement(&pSession->refCnt_);
	ZeroMemory(&(pSession->sendOverlapped.overlapped), sizeof(WSAOVERLAPPED));
	pSession->sendOverlapped.why = OVERLAPPED_REASON::SEND;
	int iSendRet = WSASend(pSession->sock_, wsa, i, nullptr, 0, &pSession->sendOverlapped.overlapped, nullptr);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == TRUE)
			{
				CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->refCnt_));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Disconnected By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

BOOL LanClient::RecvPost(LanClientSession* pSession)
{
	WSABUF wsa[2];
	wsa[0].buf = pSession->recvRB_.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB_.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB_.Buffer_;
	wsa[1].len = pSession->recvRB_.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	pSession->recvOverlapped.why = OVERLAPPED_REASON::RECV;
	DWORD flags = 0;
	InterlockedIncrement(&pSession->refCnt_);
	int iRecvRet = WSARecv(pSession->sock_, wsa, 2, nullptr, &flags, &pSession->recvOverlapped.overlapped, nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == TRUE)
			{
				CancelIoEx((HANDLE)pSession->sock_, &pSession->recvOverlapped.overlapped);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->refCnt_));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Disconnected By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

void LanClient::ReleaseSession(LanClientSession* pSession)
{
	if (InterlockedCompareExchange(&pSession->refCnt_, LanClientSession::RELEASE_FLAG | 0, 0) != 0)
		return;

	// Release �� Session�� ����ȭ ���� ����
	for (LONG i = 0; i < pSession->lSendBufNum_; ++i)
	{
		Packet* pPacket = pSession->pSendPacketArr_[i];
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	LONG size = pSession->sendPacketQ_.GetSize();
	for (LONG i = 0; i < size; ++i)
	{
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	closesocket(pSession->sock_);
	OnRelease(pSession->id_);
	// ���⼭���� �ٽ� Connect�� ���������� ������ ������ �� �Լ� ���ϴܿ� �;���
	idxStack_.Push((short)(pSession - pSessionArr_));
	InterlockedDecrement(&lSessionNum_);
}

void LanClient::RecvProc(LanClientSession* pSession, int numberOfBytesTransferred)
{
	using LanHeader = Packet::LanHeader;
	pSession->recvRB_.MoveInPos(numberOfBytesTransferred);
	while (1)
	{
		Packet::LanHeader header;
		if (pSession->recvRB_.Peek((char*)&header, sizeof(LanHeader)) == 0)
			break;

		if (pSession->recvRB_.GetUseSize() < sizeof(LanHeader) + header.payloadLen_)
			break;

		SmartPacket sp = PACKET_ALLOC(Lan);

		// ����ȭ���ۿ� ������뾲��
		memcpy(sp->pBuffer_, &header, sizeof(Packet::LanHeader));
		pSession->recvRB_.MoveOutPos(sizeof(LanHeader));

		// ���̷ε� ����ȭ���۷� ����
		pSession->recvRB_.Dequeue(sp->GetPayloadStartPos<Lan>(), header.payloadLen_);
		sp->MoveWritePos(header.payloadLen_);

		pSession->lastRecvTime = GetTickCount64();
		OnRecv(pSession->id_, sp.GetPacket());
	}
	RecvPost(pSession);
}

void LanClient::SendProc(LanClientSession* pSession, DWORD dwNumberOfBytesTransferred)
{
	LONG sendBufNum = InterlockedExchange(&pSession->lSendBufNum_, 0);
	for (LONG i = 0; i < sendBufNum; ++i)
	{
		Packet* pPacket = pSession->pSendPacketArr_[i];
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}
	InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
	SendPost(pSession);
}

void LanClient::ConnectProc(LanClientSession* pSession)
{
	InterlockedIncrement(&pSession->refCnt_);
	InterlockedAnd(&pSession->refCnt_, ~LanClientSession::RELEASE_FLAG);
	InterlockedIncrement(&lSessionNum_);

	setsockopt(pSession->sock_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	pSession->Init(InterlockedIncrement(&ullIdCounter_) - 1, (short)(pSession - pSessionArr_));

	OnConnect(pSession->id_);
	RecvPost(pSession);
}


unsigned __stdcall LanClient::IOCPWorkerThread(LPVOID arg)
{
#pragma warning(disable : 4244)
	srand(time(nullptr));
#pragma warning(default: 4244)
	LanClient* pLanClient = (LanClient*)arg;
	while (1)
	{
		MYOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		LanClientSession* pSession = nullptr;
		bool bContinue = false;
		bool bConnectSuccess = true;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pLanClient->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);
		do
		{
			// ������ RST�� ���� dwNOBT == 0 && pOverlapped->why == OVERLAPEED::REASON == RECV�� ���� ��� ���ص� ��
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			if (!bGQCSRet && pOverlapped)
			{
				DWORD errCode = WSAGetLastError();
				if (pOverlapped->why == OVERLAPPED_REASON::CONNECT)
				{
					if (errCode != ERROR_CONNECTION_REFUSED)
						LOG(L"ERROR", ERR, TEXTFILE, L"ConnectEx Failed ErrCode : %u", WSAGetLastError());

					bContinue = true;
					closesocket(pSession->sock_);
					pLanClient->OnConnectFailed(pSession->id_);

					if (pLanClient->bAutoReconnect_)
					{
						if (InterlockedDecrement(&pSession->reConnectCnt_) > 0)
						{
							HANDLE hTimer;
							pLanClient->ReconnectQ_.Enqueue(pSession);
							if (0 == CreateTimerQueueTimer(&hTimer, NULL, ReconnectTimer, (PVOID)pLanClient, pLanClient->autoReconnectInterval_, 0, WT_EXECUTEDEFAULT))
							{
								DWORD errCode = GetLastError();
								LOG(L"ERROR", ERR, TEXTFILE, L"Make Reconnect TimerQueueTimer Failed ErrCode : %u", GetLastError());
								__debugbreak();
							}
						}
						else
						{
							pLanClient->idxStack_.Push((short)(pSession - pLanClient->pSessionArr_)); // �ٷ� �Ʒ����� �Լ����� �����־� ���������� ��ٷ� �ٽ� Connect �ص� ������
							pLanClient->OnAutoResetAllFailed();
						}
					}
					else
						pLanClient->idxStack_.Push((short)(pSession - pLanClient->pSessionArr_));

					continue;
				}
				else
					break;
			}

			switch (pOverlapped->why)
			{
			case OVERLAPPED_REASON::SEND:
				pLanClient->SendProc(pSession, dwNOBT);
				break;
			case OVERLAPPED_REASON::RECV:
				if (!(bGQCSRet && dwNOBT == 0))
				{
					pLanClient->RecvProc(pSession, dwNOBT);
				}
				break;

			case OVERLAPPED_REASON::UPDATE:
				break;

			case OVERLAPPED_REASON::POST:
				break;

			case OVERLAPPED_REASON::SEND_WORKER:
				break;

			case OVERLAPPED_REASON::CONNECT:
				pLanClient->ConnectProc(pSession);
				break;

			case OVERLAPPED_REASON::RECONNECT:
				bContinue = true;
				pLanClient->ConnectPost(true, pSession, &pSession->sockAddrIn_); // ConnectPost���� ������ �õ��� IP�� �����.
				break;

			default:
				__debugbreak();
			}

		} while (0);

		if (bContinue)
			continue;

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
		{
			pLanClient->ReleaseSession(pSession);
		}
	}
	return 0;
}

bool LanClient::SetLinger(SOCKET sock)
{
	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = 1;
	return setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger)) == 0;
}

bool LanClient::SetZeroCopy(SOCKET sock)
{
	DWORD dwSendBufSize = 0;
	return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize)) == 0;
}

bool LanClient::SetReuseAddr(SOCKET sock)
{
	DWORD option = 1;
	return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, sizeof(option)) == 0;
}

bool LanClient::SetClientBind(SOCKET sock)
{
	SOCKADDR_IN addr;
	ZeroMemory(&addr, sizeof(SOCKADDR_IN));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = ::htonl(INADDR_ANY);
	addr.sin_port = ::htons(0);

	if (bind(sock, reinterpret_cast<const SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		DWORD errCode = WSAGetLastError();
		__debugbreak();
		return false;
	}
	return true;
}


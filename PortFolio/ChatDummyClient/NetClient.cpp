#include <WS2tcpip.h>
#include <locale>

#include "NetClient.h"
#include "Logger.h"
#include "Parser.h"

#include "Assert.h"
#include "NetClientSession.h"
#include "UpdateBase.h"

#pragma comment(lib,"LoggerMT.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"TextParser.lib")
#pragma comment(lib,"Winmm.lib")

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

void NetClient::ConnectProc(NetClientSession* pSession)
{
	InterlockedIncrement(&pSession->refCnt_);
	InterlockedAnd(&pSession->refCnt_, ~NetClientSession::RELEASE_FLAG);

	if (0 > setsockopt(pSession->sock_, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0))
		__debugbreak();

	//SetLinger(pSession->sock_);
	//SetZeroCopy(pSession->sock_);
	pSession->Init(InterlockedIncrement(&ullIdCounter_) - 1, (short)(pSession - pSessionArr_));

	OnConnect(pSession->id_);
	RecvPost(pSession);
}

void NetClient::DisconnectProc(NetClientSession* pSession)
{
	// Release 될 Session의 직렬화 버퍼 정리
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

	OnRelease(pSession->id_);
	// 여기서부터 다시 Connect가 가능해지기 때문에 무조건 이 함수 최하단에 와야함
	DisconnectStack_.Push((short)(pSession - pSessionArr_));
	InterlockedDecrement(&lSessionNum_);
}

void CALLBACK ReconnectTimer(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	static MYOVERLAPPED ReconnectOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::RECONNECT };

	NetClient* pNetClient = (NetClient*)lpParam;
	NetClientSession* pSession = pNetClient->ReconnectQ_.Dequeue().value();
	PostQueuedCompletionStatus(pNetClient->hcp_, 2, (ULONG_PTR)pSession, &ReconnectOverlapped.overlapped);
}

unsigned __stdcall NetClient::IOCPWorkerThread(LPVOID arg)
{
#pragma warning(disable : 4244)
	srand(time(nullptr));
#pragma warning(default: 4244)
	NetClient* pNetClient = (NetClient*)arg;
	while (1)
	{
		MYOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		NetClientSession* pSession = nullptr;
		bool bContinue = false;
		bool bConnectSuccess = true;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pNetClient->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);
		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			if (!bGQCSRet && pOverlapped)
			{
				DWORD errCode = WSAGetLastError();
				if (errCode != 0)
					LOG(L"ERROR", ERR, TEXTFILE, L"ConnectEx Failed ErrCode : %u", WSAGetLastError());

				if (pOverlapped->why == OVERLAPPED_REASON::CONNECT)
				{
					bContinue = true;
					closesocket(pSession->sock_);
					pNetClient->OnConnectFailed(pSession->id_); // 사용자가 정하기에 따라 바로 다시걸지 아니면 그냥 인덱스 스택에 삽입할지 결정

					if (pNetClient->bAutoReconnect_)
					{
						if (InterlockedDecrement(&pSession->reConnectCnt_) > 0)
						{
							HANDLE hTimer;
							pNetClient->ReconnectQ_.Enqueue(pSession);
							if (0 == CreateTimerQueueTimer(&hTimer, NULL, ReconnectTimer, (PVOID)pNetClient, pNetClient->autoReconnectInterval_, 0, WT_EXECUTEDEFAULT))
							{
								DWORD errCode = GetLastError();
								__debugbreak();
							}
						}
						else
						{
							pNetClient->DisconnectStack_.Push((short)(pSession - pNetClient->pSessionArr_)); // 일부러 앞에둠
							pNetClient->OnAutoResetAllFailed();
						}
					}
					else
						pNetClient->DisconnectStack_.Push((short)(pSession - pNetClient->pSessionArr_));
					continue;
				}

				else
					break;
			}

			switch (pOverlapped->why)
			{
			case OVERLAPPED_REASON::SEND:
			{
				pNetClient->SendProc(pSession, dwNOBT);
				break;
			}
			case OVERLAPPED_REASON::RECV:
				if (!(bGQCSRet && dwNOBT == 0))
				{
					pNetClient->RecvProc(pSession, dwNOBT);
				}
				break;

			case OVERLAPPED_REASON::UPDATE:
			{
				((UpdateBase*)(pSession))->Update();
				bContinue = true;
				break;
			}

			case OVERLAPPED_REASON::POST:
				break;

			case OVERLAPPED_REASON::SEND_WORKER:
				break;

			case OVERLAPPED_REASON::CONNECT:
			{
				pNetClient->ConnectProc(pSession);
				break;
			}

			case OVERLAPPED_REASON::RECONNECT:
			{
				bContinue = true;
				pNetClient->ConnectPost(true, pSession, &pSession->sockAddrIn_); // ConnectPost에서 이전에 시도한 IP는 저장됨.
				break;
			}

			default:
				__debugbreak();
			}

		} while (0);

		if (bContinue)
			continue;

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			pNetClient->ReleaseSession(pSession);
	}
	return 0;
}

bool NetClient::SetLinger(SOCKET sock)
{
	linger linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	return setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger)) == 0;
}

bool NetClient::SetZeroCopy(SOCKET sock)
{
	DWORD dwSendBufSize = 0;
	return setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize)) == 0;
}

bool NetClient::SetReuseAddr(SOCKET sock)
{
	DWORD option = 1;
	return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, sizeof(option)) == 0;
}

bool NetClient::SetClientBind(SOCKET sock)
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

NetClient::NetClient(const BOOL bAutoReconnect, const LONG autoReconnectCnt, const LONG autoReconnectInterval, BOOL bUseMemberSockAddrIn, const WCHAR* pIP, const USHORT port, const DWORD iocpWorkerThreadNum,
	const DWORD cunCurrentThreadNum, const LONG maxSession, const BYTE packetCode, const BYTE packetFixedKey)
	:bAutoReconnect_{ bAutoReconnect }, autoReconnectCnt_{ autoReconnectCnt }, autoReconnectInterval_{ autoReconnectInterval }, IOCP_WORKER_THREAD_NUM_{ iocpWorkerThreadNum }, IOCP_ACTIVE_THREAD_NUM_{ cunCurrentThreadNum }, maxSession_{ maxSession },
	pSessionArr_{ new NetClientSession[maxSession] }, hIOCPWorkerThreadArr_{ new HANDLE[iocpWorkerThreadNum] }, hcp_{ CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, cunCurrentThreadNum) }
{
	timeBeginPeriod(1);

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET dummySock = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_LOG(WsaIoctlWrapper(dummySock, WSAID_CONNECTEX, (LPVOID*)&lpfnConnectExPtr_) == false, L"WSAIoCtl ConnectEx Failed");
	closesocket(dummySock);

	if (bUseMemberSockAddrIn)
	{
		// sockAddr_ 초기화, ConnectEx에서 사용
		ZeroMemory(&sockAddr_, sizeof(sockAddr_));
		sockAddr_.sin_family = AF_INET;
		InetPtonW(AF_INET, pIP, &sockAddr_.sin_addr);
		sockAddr_.sin_port = htons(port);
	}

	Packet::PACKET_CODE = packetCode;
	Packet::FIXED_KEY = packetFixedKey;

	// IOCP 핸들 검사 
	ASSERT_LOG(hcp_ == NULL, L"CreateIoCompletionPort Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	// 상위 17비트를 못쓰고 상위비트가 16개 이하가 되는날에는 뻑나라는 큰그림이다.
	ASSERT_FALSE_LOG(CAddressTranslator::CheckMetaCntBits(), L"LockFree 17bits Over");

	for (int i = maxSession_ - 1; i >= 0; --i)
		DisconnectStack_.Push(i);

	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, CREATE_SUSPENDED, nullptr);
		ASSERT_ZERO_LOG(hIOCPWorkerThreadArr_[i], L"MAKE WorkerThread Fail");
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", IOCP_WORKER_THREAD_NUM_);
}


NetClient::~NetClient()
{
	WSACleanup();
}

void NetClient::InitialConnect(SOCKADDR_IN* pSockAddrIn)
{
	while (true)
	{
		auto opt = DisconnectStack_.Pop();
		if (opt.has_value() == false)
			return;

		NetClientSession* pSession = pSessionArr_ + opt.value();
		ConnectPost(false, pSession, pSockAddrIn);
	}
}

bool NetClient::Connect(bool bRetry, SOCKADDR_IN* pSockAddrIn)
{
	auto opt = DisconnectStack_.Pop();
	if (opt.has_value() == false)
	{
		__debugbreak();
		return false;
	}

	NetClientSession* pSession = pSessionArr_ + opt.value();
	ConnectPost(bRetry, pSession, pSockAddrIn);
	return true;
}


void NetClient::SendPacket(ULONGLONG id, SmartPacket& sendPacket)
{
	NetClientSession* pSession = pSessionArr_ + NetClientSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & NetClientSession::RELEASE_FLAG) == NetClientSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE 완료후 다시 세션에 대한 초기화가 완료된경우 즉 재활용
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 인코딩
	sendPacket->SetHeader<Net>();
	sendPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(sendPacket.GetPacket());
	SendPost(pSession);
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void NetClient::Disconnect(ULONGLONG id)
{
	NetClientSession* pSession = pSessionArr_ + NetClientSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// RELEASE진행중 혹은 진행완료
	if ((IoCnt & NetClientSession::RELEASE_FLAG) == NetClientSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// RELEASE후 재활용까지 되엇을때
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// Disconnect 1회 제한
	if (InterlockedExchange((LONG*)&pSession->bDisconnectCalled_, TRUE) == TRUE)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 여기 도달햇다면 같은 세션에 대해서 RELEASE 조차 호출되지 않은상태임이 보장된다
	CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);
	CancelIoEx((HANDLE)pSession->sock_, &pSession->recvOverlapped.overlapped);

	// CancelIoEx호출로 인해서 RELEASE가 호출되엇어야 햇지만 위에서의 InterlockedIncrement 때문에 호출이 안된 경우 업보청산
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

bool NetClient::ConnectPost(bool bRetry, NetClientSession* pSession, SOCKADDR_IN* pSockAddrIn)
{
	if (!bRetry)
	{
		memcpy(&pSession->sockAddrIn_, pSockAddrIn, sizeof(SOCKADDR_IN));
		InterlockedExchange(&pSession->reConnectCnt_, autoReconnectCnt_);
	}

	// 소켓 생성, 옵션 설정후 IOCP에 등록
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	SetClientBind(sock);

	if (NULL == CreateIoCompletionPort((HANDLE)sock, hcp_, (ULONG_PTR)pSession, 0))
	{
		DWORD errCode = WSAGetLastError();
		__debugbreak();
	}

	pSession->sock_ = sock;
	ZeroMemory(&pSession->connectOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	pSession->connectOverlapped.why = OVERLAPPED_REASON::CONNECT;
	BOOL bConnectExRet = lpfnConnectExPtr_(sock, (SOCKADDR*)pSockAddrIn, sizeof(SOCKADDR_IN), nullptr, 0, NULL, &pSession->connectOverlapped.overlapped);
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
			DisconnectStack_.Push((short)(pSession - pSessionArr_));

		LOG(L"ERROR", ERR, TEXTFILE, L"ConnectEx ErrCode : %u", errCode);
		__debugbreak();
		return false;
	}
	else
	{
		__debugbreak();
	}
	return true;
}


BOOL NetClient::SendPost(NetClientSession* pSession)
{
	DWORD dwBufferNum;
	while (1)
	{
		if (pSession->sendPacketQ_.GetSize() <= 0)
			return FALSE;

		// 현재 값을 TRUE로 바꾼다. 원래 TRUE엿다면 반환값이 TRUE일것이며 그렇다면 현재 SEND 진행중이기 때문에 그냥 빠저나간다
		// 이 조건문의 위치로 인하여 Out은 바뀌지 않을것임이 보장된다.
		// 하지만 SendPost 실행주체가 Send완료통지 스레드인 경우에는 in의 위치는 SendPacket으로 인해서 바뀔수가 있다.
		// iUseSize를 구하는 시점에서의 DirectDequeueSize의 값이 달라질수있다.
		if (InterlockedExchange((LONG*)&pSession->bSendingInProgress_, TRUE) == TRUE)
			return TRUE;

		// SendPacket에서 in을 옮겨서 UseSize가 0보다 커진시점에서 Send완료통지가 도착해서 Out을 옮기고 플래그 해제 Recv완료통지 스레드가 먼저 SendPost에 도달해 플래그를 선점한경우 UseSize가 0이나온다.
		// 여기서 flag를 다시 FALSE로 바꾸어주지 않아서 멈춤발생
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
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(Packet::NetHeader);
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

BOOL NetClient::RecvPost(NetClientSession* pSession)
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

void NetClient::ReleaseSession(NetClientSession* pSession)
{
	if (InterlockedCompareExchange(&pSession->refCnt_, NetClientSession::RELEASE_FLAG | 0, 0) != 0)
		return;

	// Release 될 Session의 직렬화 버퍼 정리
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
	// 여기서부터 다시 Connect가 가능해지기 때문에 무조건 이 함수 최하단에 와야함
	DisconnectStack_.Push((short)(pSession - pSessionArr_));
	InterlockedDecrement(&lSessionNum_);
}


void NetClient::RecvProc(NetClientSession* pSession, int numberOfBytesTransferred)
{
	using NetHeader = Packet::NetHeader;
	pSession->recvRB_.MoveInPos(numberOfBytesTransferred);
	while (1)
	{
		Packet::NetHeader header;
		if (pSession->recvRB_.Peek((char*)&header, sizeof(NetHeader)) == 0)
			break;

		if (header.code_ != Packet::PACKET_CODE)
		{
			LOG(L"RecvError", ERR, TEXTFILE, L"Network Header Invalid : %d", header.code_);
			Disconnect(pSession->id_);
			return;
		}

		if (pSession->recvRB_.GetUseSize() < sizeof(NetHeader) + header.payloadLen_)
		{
			if (header.payloadLen_ > BUFFER_SIZE)
			{
				LOG(L"RecvError", ERR, TEXTFILE, L"Network Header payloadLen(%d) is Larger Than RBSize(%d)", header.payloadLen_, BUFFER_SIZE);
				Disconnect(pSession->id_);
				return;
			}
			break;
		}

		pSession->recvRB_.MoveOutPos(sizeof(NetHeader));
		SmartPacket sp = PACKET_ALLOC(Net);
		pSession->recvRB_.Dequeue(sp->GetPayloadStartPos<Net>(), header.payloadLen_);
		sp->MoveWritePos(header.payloadLen_);
		memcpy(sp->pBuffer_, &header, sizeof(Packet::NetHeader));

		// 디코드후 체크섬 확인
		if (sp->ValidateReceived() == false)
		{
			LOG(L"RecvError", ERR, TEXTFILE, L"RecvPacket Decoding Invalid");
			Disconnect(pSession->id_);
			return;
		}

		pSession->lastRecvTime = GetTickCount64();
		OnRecv(pSession->id_, sp);
	}
	RecvPost(pSession);
}

void NetClient::SendProc(NetClientSession* pSession, DWORD dwNumberOfBytesTransferred)
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


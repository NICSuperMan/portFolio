#include <WinSock2.h>
#include <WS2tcpip.h>
#include <locale>
#include <process.h>
#include <Assert.h>
#include <Scheduler/Scheduler.h>
#include <MySqlUtil/DBWriteThread.h>
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include "LanServer.h"
#include "LanServerTimeOut.h"
#pragma comment(lib,"LoggerMt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"Winmm.lib")



template<typename T>
__forceinline T& IGNORE_CONST(const T& value)
{
	return const_cast<T&>(value);
}

LanServer::LanServer(const WCHAR* pIP, const USHORT port, const DWORD IocpWorkerThreadNum, const DWORD CuncurrentThreadNum, const BOOL bZeroCopy, const LONG maxSession, const BOOL bActivateTimeOut, const LONG userTimeOut, const DWORD timeOutCheckInterval)
	:IOCP_WORKER_THREAD_NUM_{ IocpWorkerThreadNum }, IOCP_ACTIVE_THREAD_NUM_{ CuncurrentThreadNum }, maxSession_{ maxSession }, TIME_OUT_MILLISECONDS_{ userTimeOut }, TIME_OUT_CHECK_INTERVAL_{ timeOutCheckInterval }, hShutDownEvent_{ CreateEvent(NULL,FALSE,FALSE,NULL) }
{
	std::locale::global(std::locale(""));
	timeBeginPeriod(1);

	WSADATA wsa;
	ASSERT_NON_ZERO_LOG(WSAStartup(MAKEWORD(2, 2), &wsa), L"WSAStartUp Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");

	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, CuncurrentThreadNum);
	ASSERT_NULL_LOG(hcp_, L"CreateIoCompletionPort Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	hListenSock_ = socket(AF_INET, SOCK_STREAM, 0);
	ASSERT_INVALID_SOCKET_LOG(hListenSock_, L"MAKE Listen SOCKET Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET OK");

	// bind
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));

	ULONG ip;
	InetPtonW(AF_INET, pIP, &ip);
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = ip;
	serveraddr.sin_port = htons(port);
	ASSERT_SOCKET_ERROR_LOG(bind(hListenSock_, (SOCKADDR*)&serveraddr, sizeof(serveraddr)), "bind Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind OK");

	ASSERT_SOCKET_ERROR_LOG(listen(hListenSock_, SOMAXCONN), "listen Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen() OK");

	linger linger;
	linger.l_linger = 0;
	linger.l_onoff = 1;
	ASSERT_NON_ZERO_LOG(setsockopt(hListenSock_, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger)), "Linger Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"linger() OK");

	if (bZeroCopy == TRUE)
	{
		DWORD dwSendBufSize = 0;
		setsockopt(hListenSock_, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize));
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"ZeroByte Send OK");
	}
	else
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"NO ZeroByte Send");
	}

	hIOCPWorkerThreadArr_ = new HANDLE[IOCP_WORKER_THREAD_NUM_];
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, CREATE_SUSPENDED, nullptr);
		ASSERT_ZERO_LOG(hIOCPWorkerThreadArr_[i], "MAKE WorkerThread Fail ErrCode");
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", CuncurrentThreadNum);

	// 상위 17비트를 못쓰고 상위비트가 16개 이하가 되는날에는 뻑나라는 큰그림이다.
	ASSERT_FALSE_LOG(CAddressTranslator::CheckMetaCntBits(), L"LockFree 17bits Over");

	pSessionArr_ = new LanSession[maxSession_];
	for (int i = maxSession_ - 1; i >= 0; --i)
		DisconnectStack_.Push(i);

	hAcceptThread_ = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, CREATE_SUSPENDED, nullptr);
	ASSERT_ZERO_LOG(hAcceptThread_, L"MAKE AcceptThread Fail");
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE AccpetThread OK!");

	Scheduler::Init();

	// 모니터링 서버 타임아웃(대부분의 경우 필요없을거같지만 원장님이 악의적인 공격시 혹시라도 OnConnectionRequest에서 못끊어낼까봐 만듬)
	if (bActivateTimeOut)
	{
		pLanTimeOut_ = new LanServerTimeOut{ (DWORD)timeOutCheckInterval,hcp_,3,this };
		Scheduler::Register_UPDATE(pLanTimeOut_);
	}

}

void LanServer::SendPacket(ULONGLONG id, SmartPacket& sendPacket)
{
	LanSession* pSession = pSessionArr_ + LanSession::GET_SESSION_INDEX(id);
	LONG IoCnt = InterlockedIncrement(&pSession->refCnt_);
	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & LanSession::RELEASE_FLAG) == LanSession::RELEASE_FLAG)
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

	sendPacket->SetHeader<Lan>();
	sendPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(sendPacket.GetPacket());
	SendPost(pSession);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanServer::SendPacket(ULONGLONG id, Packet* pPacket)
{
	LanSession* pSession = pSessionArr_ + LanSession::GET_SESSION_INDEX(id);
	LONG IoCnt = InterlockedIncrement(&pSession->refCnt_);
	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & LanSession::RELEASE_FLAG) == LanSession::RELEASE_FLAG)
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

	pPacket->SetHeader<Lan>();
	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);
	SendPost(pSession);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanServer::Disconnect(ULONGLONG id)
{
	LanSession* pSession = pSessionArr_ + LanSession::GET_SESSION_INDEX(id);
	LONG IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// RELEASE진행중 혹은 진행완료
	if ((IoCnt & LanSession::RELEASE_FLAG) == LanSession::RELEASE_FLAG)
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
	if ((BOOL)InterlockedExchange((LONG*)&pSession->bDisconnectCalled_, TRUE) == TRUE)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 여기 도달햇다면 같은 세션에 대해서 RELEASE 조차 호출되지 않은상태임이 보장된다
	CancelIoEx((HANDLE)pSession->sock_, nullptr);

	// CancelIoEx호출로 인해서 RELEASE가 호출되엇어야 햇지만 위에서의 InterlockedIncrement 때문에 호출이 안된 경우 업보청산
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void LanServer::WaitUntilShutDown()
{
	WaitForSingleObject(hShutDownEvent_, INFINITE);
	ShutDown();
}

const WCHAR* LanServer::GetSessionIP(ULONGLONG id)
{
	return (pSessionArr_ + LanSession::GET_SESSION_INDEX(id))->ip_;
}

const USHORT LanServer::GetSessionPort(ULONGLONG id)
{
	return (pSessionArr_ + LanSession::GET_SESSION_INDEX(id))->port_;
}

void LanServer::ShutDown()
{
	// 워커스레드에서 호출한경우 안됨, 워커에서는 RequestShutDown을 호출해야함
	HANDLE hDebug = GetCurrentThread();
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
	{
		if (hIOCPWorkerThreadArr_[i] == hDebug)
		{
			LOG(L"ERROR", ERR, CONSOLE, L"WORKER Call Shutdown Must Have To Call RequestShutDown", Packet::packetPool_.capacity_, Packet::packetPool_.size_);
			LOG(L"ERROR", ERR, TEXTFILE, L"WORKER Call Shutdown Must Have To Call RequestShutDown", Packet::packetPool_.capacity_, Packet::packetPool_.size_);
			__debugbreak();
		}
	}
	// 리슨소켓을 닫아서 Accept를 막는다
	closesocket(hListenSock_);
	WaitForSingleObject(hAcceptThread_, INFINITE);
	CloseHandle(hAcceptThread_);

	//세션 0될때까지 돌린다
	while (InterlockedXor(&lSessionNum_, 0) != 0)
	{
		for (int i = 0; i < maxSession_; ++i)
		{
			CancelIoEx((HANDLE)pSessionArr_[i].sock_, nullptr);
			InterlockedExchange((LONG*)&pSessionArr_[i].bDisconnectCalled_, TRUE);
		}
	}

	// 더이상 PQCS는 들어오지 않으므로 UpdateBase* 를 PQCS로 쏘는것을 막기위해 Timer스레드를 제거한다
	// 만약 랜서버나, 넷서버중 누군가 먼저 타이머스레드를 종료시키고 핸들을 닫더라도 Wait_Failed에 대한 예외처리를 하지않음으로서 중복호출을 허용한다
	// 왜냐하면 랜서버 혹은 넷서버 둘중 하나만 잇는 서버의 경우에 같은코드를 쓰기 위해서임
	Scheduler::Release_SchedulerThread();

	// 마지막 DB등에 대한 잔여분을 처리한다
	OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown();

	// 워커스레드를 종료하기위한 PQCS를 쏘고 대기한다
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		PostQueuedCompletionStatus(hcp_, 0, 0, 0);

	WaitForMultipleObjects(IOCP_WORKER_THREAD_NUM_, hIOCPWorkerThreadArr_, TRUE, INFINITE);
	OnResourceCleanAtShutDown();

	CloseHandle(hcp_);
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		CloseHandle(hIOCPWorkerThreadArr_[i]);
	delete[] pSessionArr_;
	CloseHandle(hShutDownEvent_);
}

void LanServer::RequestShutDown()
{
	SetEvent(hShutDownEvent_);
}

BOOL LanServer::RecvPost(LanSession* pSession)
{
	WSABUF wsa[2];
	wsa[0].buf = pSession->recvRB_.GetWriteStartPtr();
	wsa[0].len = pSession->recvRB_.DirectEnqueueSize();
	wsa[1].buf = pSession->recvRB_.Buffer_;
	wsa[1].len = pSession->recvRB_.GetFreeSize() - wsa[0].len;

	ZeroMemory(&pSession->recvOverlapped, sizeof(WSAOVERLAPPED));
	pSession->recvOverlapped.why = OVERLAPPED_REASON::RECV;
	DWORD flags = 0;
	InterlockedIncrement(&pSession->refCnt_);
	int iRecvRet = WSARecv(pSession->sock_, wsa, 2, nullptr, &flags, (LPWSAOVERLAPPED) & (pSession->recvOverlapped), nullptr);
	if (iRecvRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == TRUE)
			{
				CancelIoEx((HANDLE)pSession->sock_, nullptr);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->refCnt_));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

BOOL LanServer::SendPost(LanSession* pSession)
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
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(Packet::LanHeader);
		pSession->pSendPacketArr_[i] = pPacket;
	}

	InterlockedExchange(&pSession->lSendBufNum_, i);
	InterlockedAdd64(&sendTPS_, i);
	InterlockedIncrement(&pSession->refCnt_);
	ZeroMemory(&(pSession->sendOverlapped.overlapped), sizeof(WSAOVERLAPPED));
	pSession->sendOverlapped.why = OVERLAPPED_REASON::SEND;
	int iSendRet = WSASend(pSession->sock_, wsa, i, nullptr, 0, (LPWSAOVERLAPPED) & (pSession->sendOverlapped), nullptr);
	if (iSendRet == SOCKET_ERROR)
	{
		DWORD dwErrCode = WSAGetLastError();
		if (dwErrCode == WSA_IO_PENDING)
		{
			if (pSession->bDisconnectCalled_ == TRUE)
			{
				CancelIoEx((HANDLE)pSession->sock_, nullptr);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->refCnt_));
		if (dwErrCode == WSAECONNRESET)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

void LanServer::ReleaseSession(LanSession* pSession)
{
	if (InterlockedCompareExchange(&pSession->refCnt_, LanSession::RELEASE_FLAG | 0, 0) != 0)
		return;

	// TimeOut때문에 이리 만듬
	pSession->lastRecvTime = 0;

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
#pragma warning(disable : 26815)
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
#pragma warning(default : 26815)
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	closesocket(pSession->sock_);
	if (pSession->sendPacketQ_.GetSize() > 0)
		__debugbreak();

	// OnRelease와 idx 푸시 순서가 바뀌어서 JOB_OnRelease 도착 이전에 새로운 플레이어에 대해 JOB_On_ACCEPT가 중복으로 도착햇음
	OnRelease(pSession->id_);
	DisconnectStack_.Push((short)(pSession - pSessionArr_));
	InterlockedIncrement(&disconnectTPS_);
	InterlockedDecrement(&lSessionNum_);
}

void LanServer::RecvProc(LanSession* pSession, DWORD dwNumberOfBytesTransferred)
{
	using LanHeader = Packet::LanHeader;
	pSession->recvRB_.MoveInPos(dwNumberOfBytesTransferred);
	while (1)
	{
		LanHeader header;
		if (pSession->recvRB_.Peek((char*)&header, sizeof(LanHeader)) == 0)
			break;

		if (pSession->recvRB_.GetUseSize() < sizeof(LanHeader) + header.payloadLen_)
		{
			if (header.payloadLen_ > BUFFER_SIZE)
			{
				Disconnect(pSession->id_);
				return;
			}
			break;
		}

		pSession->recvRB_.MoveOutPos(sizeof(LanHeader));

		Packet* pPacket = PACKET_ALLOC(Lan);
		pSession->recvRB_.Dequeue(pPacket->GetPayloadStartPos<Lan>(), header.payloadLen_);
		pPacket->MoveWritePos(header.payloadLen_);
		memcpy(pPacket->pBuffer_, &header, sizeof(Packet::LanHeader));

		pSession->lastRecvTime = GetTickCount64();
		InterlockedIncrement64(&recvTPS_);
		OnRecv(pSession->id_, pPacket);
	}
	RecvPost(pSession);
}

void LanServer::SendProc(LanSession* pSession, DWORD dwNumberOfBytesTransferred)
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

unsigned __stdcall LanServer::AcceptThread(LPVOID arg)
{
	SOCKET clientSock;
	SOCKADDR_IN clientAddr;
	int addrlen = sizeof(clientAddr);
	LanServer* pLanServer = (LanServer*)arg;

	while (1)
	{
		clientSock = accept(pLanServer->hListenSock_, (SOCKADDR*)&clientAddr, &addrlen);
		InterlockedIncrement((LONG*)&pLanServer->acceptCounter_);

		if (clientSock == INVALID_SOCKET)
		{
			DWORD dwErrCode = WSAGetLastError();
			if (dwErrCode != WSAEINTR && dwErrCode != WSAENOTSOCK)
			{
				__debugbreak();
			}
			return 0;
		}

		WCHAR ip[16];
		InetNtop(AF_INET, &clientAddr.sin_addr, ip, _countof(ip));
		USHORT port = ntohs(clientAddr.sin_port);

		if (!pLanServer->OnConnectionRequest(ip, port))
		{
			closesocket(clientSock);
			continue;
		}

		// maxSession 만큼접속해서 인덱스 자리없으면 내보내기
		auto&& opt = pLanServer->DisconnectStack_.Pop();
		if (!opt.has_value())
		{
			closesocket(clientSock);
			continue;
		}
		short idx = opt.value();

		InterlockedIncrement((LONG*)&pLanServer->lSessionNum_);
		LanSession* pSession = pLanServer->pSessionArr_ + idx;
		pSession->Init(clientSock, pLanServer->ullIdCounter, idx);

		CreateIoCompletionPort((HANDLE)pSession->sock_, pLanServer->hcp_, (ULONG_PTR)pSession, 0);
		++pLanServer->ullIdCounter;

		// ip와 포트를 세션에 저장
		memcpy(pSession->ip_, ip, sizeof(WCHAR) * 16);
		pSession->port_ = port;

		InterlockedIncrement(&pSession->refCnt_);
		InterlockedAnd(&pSession->refCnt_, ~LanSession::RELEASE_FLAG);

		pLanServer->OnAccept(pSession->id_);
		pLanServer->RecvPost(pSession);

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			pLanServer->ReleaseSession(pSession);
	}
	return 0;
}

unsigned __stdcall LanServer::IOCPWorkerThread(LPVOID arg)
{
	LanServer* pLanServer = (LanServer*)arg;
	while (1)
	{
		MYOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		LanSession* pSession = nullptr;
		bool bContinue = false;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pLanServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);
		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			//정상종료
			if (bGQCSRet && dwNOBT == 0)
				break;

			if (!bGQCSRet && pOverlapped)
				break;

			switch (pOverlapped->why)
			{
			case OVERLAPPED_REASON::SEND:
				pLanServer->SendProc(pSession, dwNOBT);
				break;

			case OVERLAPPED_REASON::RECV:
				pLanServer->RecvProc(pSession, dwNOBT);
				break;

			//case OVERLAPPED_REASON::TIMEOUT:
			//	bContinue = true;
			//	break;

			case OVERLAPPED_REASON::UPDATE:
				((UpdateBase*)(pSession))->Update();
				bContinue = true;
				break;

			case OVERLAPPED_REASON::POST:
				pLanServer->OnPost(pSession);
				bContinue = true;
				break;

			case OVERLAPPED_REASON::SEND_WORKER:
				pLanServer->SendPost(pSession);
				InterlockedExchange((LONG*)&pSession->bSendingAtWorker_, FALSE);
				break;

			case OVERLAPPED_REASON::CONNECT: // 안씀 (랜넷 클라에서씀)
				break;

			//case OVERLAPPED_REASON::DB_WRITE:
			//	((DBWriteThreadBase*)pSession)->ProcessDBWrite();
			//	bContinue = true;
			//	break;

			default:
				__debugbreak();
			}

		} while (0);

		if (bContinue)
			continue;

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			pLanServer->ReleaseSession(pSession);
	}
	return 0;
}

#include <Winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <process.h>
#include "CLockFreeQueue.h"
#include "RingBuffer.h"
#include "GameSession.h"
#include "Packet.h"
#include "ErrType.h"
#include "Logger.h"
#include "Parser.h"
#include <locale>
#include "Scheduler.h"
#include "GameServer.h"
#include "ContentsBase.h"
#include "Assert.h"
#pragma comment(lib,"LoggerMt.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"TextParser.lib")
#pragma comment(lib,"Winmm.lib")


#define GetPlayerPtr(idx) ((void*)((char*)(pGameServer->pPlayerArr_) + pGameServer->playerSize_ * idx))


GameServer::GameServer(WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, LONG maxUser, LONG playerSize, BYTE packetCode, BYTE packetfixedKey)
	:IOCP_WORKER_THREAD_NUM_{ iocpWorkerNum }, IOCP_ACTIVE_THREAD_NUM_{ cunCurrentThreadNum },maxSession_{ maxSession }, maxPlayer_{ maxUser }, hShutDownEvent_{ CreateEvent(NULL,FALSE,FALSE,NULL) }
{
	timeBeginPeriod(1);
	// 인코딩 디코딩 위한 설정
	Packet::PACKET_CODE = packetCode;
	Packet::FIXED_KEY = packetfixedKey;

	int retval;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"WSAStartUp OK!");

	hcp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, iocpWorkerNum);
	if (!hcp_)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"CreateIoCompletionPort Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"Create IOCP OK!");

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// Listen Socket 생성
	hListenSock_ = socket(AF_INET, SOCK_STREAM, 0);
	if (hListenSock_ == INVALID_SOCKET)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}

	pSessionArr_ = new GameSession[maxSession_];

	// 리슨소켓을 IOCP에 등록해야 AcceptEx에 대한 완료통지를 받을수 잇음
	CreateIoCompletionPort((HANDLE)hListenSock_, hcp_, (ULONG_PTR)pSessionArr_, NULL);
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE Listen SOCKET OK");

	// bind
	ULONG ip;
	InetPtonW(AF_INET, pIP, &ip);

	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = ip;
	serveraddr.sin_port = htons(port);
	retval = bind(hListenSock_, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"bind OK");

	// listen
	retval = listen(hListenSock_, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen Fail ErrCode : %u", WSAGetLastError());
		__debugbreak();
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"listen() OK");

	// TIME_WAIT을 제거하기 위해 링거설정
	LINGER linger;
	linger.l_onoff = 1;
	linger.l_linger = 0;
	setsockopt(hListenSock_, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(LINGER));
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"linger() OK");

	if (bZeroCopy == 1)
	{
		DWORD dwSendBufSize = 0;
		setsockopt(hListenSock_, SOL_SOCKET, SO_SNDBUF, (char*)&dwSendBufSize, sizeof(dwSendBufSize));
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"ZeroByte Send OK");
	}
	else
	{
		LOG(L"ONOFF", SYSTEM, TEXTFILE, L"NO ZeroByte Send");
	}

	hIOCPWorkerThreadArr_ = new HANDLE[iocpWorkerNum];
	for (DWORD i = 0; i < iocpWorkerNum; ++i)
	{
		hIOCPWorkerThreadArr_[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, CREATE_SUSPENDED, nullptr);
		if (!hIOCPWorkerThreadArr_[i])
		{
			LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE WorkerThread Fail ErrCode : %u", WSAGetLastError());
			__debugbreak();
		}
	}
	LOG(L"ONOFF", SYSTEM, TEXTFILE, L"MAKE IOCP WorkerThread OK Num : %u!", iocpWorkerNum);

	// 락프리 큐 등의 next에서 상위 17비트를 못쓰고 상위비트가 16개 이하가 되는날에는 크래시
	if (!CAddressTranslator::CheckMetaCntBits())
		__debugbreak();

	// AcceptEx 함수포인터 가져오기
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	GUID guid = WSAID_ACCEPTEX;
	DWORD bytes = 0;
	if (SOCKET_ERROR == WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &pAcceptExFuncPtr, sizeof(pAcceptExFuncPtr), &bytes, NULL, NULL))
	{
		DWORD errCode = WSAGetLastError();
		LOG(L"ERROR", ERR, TEXTFILE, L"WSAIoCtl Failed ErrCode : %u", errCode);
		__debugbreak();
	}
	closesocket(sock);

	for (int i = maxSession_ - 1; i >= 0; --i)
		idxStack_.Push(i);

	SetEntirePlayerMemory(playerSize);
	Scheduler::Init();
}

GameServer::~GameServer()
{
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
	{
		CloseHandle(hIOCPWorkerThreadArr_[i]);
	}

	delete[] hIOCPWorkerThreadArr_;
	delete[] pSessionArr_;
	CloseHandle(hcp_);
	CloseHandle(hShutDownEvent_);
}

//용도 : SendFlag에 따라서 가능하다면 바로 클라이언트에게 메시지를 전송
void GameServer::SendPacket(ULONGLONG id, SmartPacket& sendPacket)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
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

	sendPacket->SetHeader<Net>();
	sendPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(sendPacket.GetPacket());
	SendPost(pSession);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

// 용도: SendFlag에 따라서 가능하다면 바로 클라이언트에게 메시지를 전송
void GameServer::SendPacket(ULONGLONG id, Packet* pPacket)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 이미 RELEASE 완료후 다시 세션이 재활용 되엇는지 체크
	if (id != pSession->id_)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 인코딩
	pPacket->SetHeader<Net>();
	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);
	SendPost(pSession);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

// 용도 : 인코딩을 컨텐츠단에서 미리 해놓고 여러스레드가 직렬화버퍼 포인터를 공유하는 경우 사용(채팅서버 멀티스레드 버전에서만 사용)
void GameServer::SendPacket_ALREADY_ENCODED(ULONGLONG id, Packet* pPacket)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
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

	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);
	SendPost(pSession);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

// 용도 : 메시지를 Send메시지 큐에 Enqueue만 하는 용도로 사용. 주로 메시지를 모아서 보내는 직렬 컨텐츠에서 활용
void GameServer::EnqPacket(ULONGLONG id, Packet* pPacket)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
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

	pPacket->SetHeader<Net>();
	pPacket->IncreaseRefCnt();
	pSession->sendPacketQ_.Enqueue(pPacket);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void GameServer::SetLogin(ULONGLONG id)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// 이미 RELEASE 진행중이거나 RELEASE된 경우
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
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

	InterlockedExchange((LONG*)&pSession->bLogin_, TRUE);

	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

void GameServer::WaitUntilShutDown()
{
	WaitForSingleObject(hShutDownEvent_, INFINITE);
	ShutDown();
}

void GameServer::ShutDown()
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
	InterlockedExchange((LONG*)&bStopAccept, TRUE);
	closesocket(hListenSock_);

	//걸린 AcceptEx도 없고, 남은 세션도 0될때까지 돌린다
	while (InterlockedXor(&acceptAllocedCnt_, 0) != 0 && InterlockedXor(&lSessionNum_, 0) != 0)
	{
		for (int i = 0; i < maxSession_; ++i)
		{
			GameSession* pSession = pSessionArr_ + i;
			CancelIoEx((HANDLE)pSession->sock_, &pSession->recvOverlapped.overlapped);
			CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);
			InterlockedExchange((LONG*)&pSessionArr_[i].bDisconnectCalled_, TRUE);
		}
	}

	// 더이상 PQCS는 들어오지 않으므로 UpdateBase* 를 PQCS로 쏘는것을 막기위해 Timer스레드를 제거한다
	Scheduler::Release_SchedulerThread();

	// 마지막 DB등에 대한 잔여분을 처리할 PQCS등을 여기서 쏜다
	OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown();

	// 워커스레드를 종료하기위한 PQCS를 쏘고 대기한다
	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		PostQueuedCompletionStatus(hcp_, 0, 0, 0);

	WaitForMultipleObjects(IOCP_WORKER_THREAD_NUM_, hIOCPWorkerThreadArr_, TRUE, INFINITE);
}

void GameServer::RequestShutDown()
{
	SetEvent(hShutDownEvent_);
}

ULONGLONG GameServer::GetSessionID(const void* pPlayer)
{
	short idx = (short)(((ULONG_PTR)pPlayer - (ULONG_PTR)pPlayerArr_) / playerSize_);
	return ((GameSession*)(pSessionArr_ + idx))->id_;
}


void GameServer::Disconnect(ULONGLONG id)
{
	GameSession* pSession = pSessionArr_ + GameSession::GET_SESSION_INDEX(id);
	long IoCnt = InterlockedIncrement(&pSession->refCnt_);

	// RELEASE진행중 혹은 진행완료
	if ((IoCnt & GameSession::RELEASE_FLAG) == GameSession::RELEASE_FLAG)
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
	if ((bool)InterlockedExchange((LONG*)&pSession->bDisconnectCalled_, TRUE) == TRUE)
	{
		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			ReleaseSession(pSession);
		return;
	}

	// 여기 도달햇다면 같은 세션에 대해서 RELEASE 조차 호출되지 않은상태임이 보장된다
	CancelIoEx((HANDLE)pSession->sock_, &pSession->recvOverlapped.overlapped);
	CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);

	// CancelIoEx호출로 인해서 RELEASE가 호출되엇어야 햇지만 위에서의 InterlockedIncrement 때문에 호출이 안된 경우 업보청산
	if (InterlockedDecrement(&pSession->refCnt_) == 0)
		ReleaseSession(pSession);
}

const WCHAR* GameServer::GetIp(ULONGLONG sessionId)
{
	return ((GameSession*)pSessionArr_ + GameSession::GET_SESSION_INDEX(sessionId))->ip_;
}

const USHORT GameServer::GetPort(ULONGLONG sessionId)
{
	return ((GameSession*)pSessionArr_ + GameSession::GET_SESSION_INDEX(sessionId))->port_;
}


// listen의 Completion Key는 GameSession* 배열의 맨 시작위치
unsigned __stdcall GameServer::IOCPWorkerThread(LPVOID arg)
{
	GameServer* pGameServer = (GameServer*)arg;
	while (1)
	{
		MYOVERLAPPED* pOverlapped = nullptr;
		DWORD dwNOBT = 0;
		GameSession* pSession = nullptr;
		bool bContinue = false;
		BOOL bGQCSRet = GetQueuedCompletionStatus(pGameServer->hcp_, &dwNOBT, (PULONG_PTR)&pSession, (LPOVERLAPPED*)&pOverlapped, INFINITE);
		do
		{
			if (!pOverlapped && !dwNOBT && !pSession)
				return 0;

			if (dwNOBT == 0 && pOverlapped->why == OVERLAPPED_REASON::RECV)
				break;

			// 실패한 IO에 대한 완료통지(Send,Recv의 경우에는 연결이끊어진 상태라서 ReleaseSession 될것이고, Accept의경우에는 인덱스만 반환하면 됨)
			if (!bGQCSRet && pOverlapped)
			{
				if (pOverlapped->why == OVERLAPPED_REASON::ACCEPT)
				{
					DWORD errCode = WSAGetLastError();
					InterlockedDecrement((LONG*)&pGameServer->acceptAllocedCnt_);
					GameSession* pTarget = (GameSession*)((char*)pOverlapped - offsetof(GameSession, acceptOverlapped));
					pGameServer->idxStack_.Push((short)(pTarget - pSession));
					bContinue = true;

					// ShutDown을 위해 리슨소켓을 닫은 경우를 제외하고 AcceptEx가 실패하는 상황 자체가 비정상 상황임
					if (WSAECONNRESET != errCode && InterlockedXor((LONG*)&pGameServer->bStopAccept, 0) != TRUE)
					{
						LOG(L"ERROR", ERR, TEXTFILE, L"AcceptEx Failed WithOut Close Listen Socket : %u", errCode);
						__debugbreak();
					}
				}
				break;
			}

			switch (pOverlapped->why)
			{
			case OVERLAPPED_REASON::SEND:
			{
				pGameServer->SendProc(pSession, dwNOBT);
				break;
			}

			case OVERLAPPED_REASON::RECV:
			{
				pGameServer->RecvProc(pSession, dwNOBT);
				break;
			}

			case OVERLAPPED_REASON::UPDATE:
			{
				((UpdateBase*)(pSession))->Update();
				bContinue = true;
				break;
			}

			case OVERLAPPED_REASON::POST:
			{
				pGameServer->OnPost(pSession);
				bContinue = true;
				break;
			}

			case OVERLAPPED_REASON::SEND_WORKER:
			{
				pGameServer->SendPost(pSession);
				break;
			}

			case OVERLAPPED_REASON::ACCEPT:
			{
				// CompletionKey를 TargetSession으로 변경
				pSession = (GameSession*)((char*)(pOverlapped) - offsetof(GameSession, acceptOverlapped));
				pGameServer->AcceptProc(pSession);
				break;
			}

			case OVERLAPPED_REASON::CONNECT: // 서버에서 안쓰고 클라에서만 쓰지만 JumpTable 때문에 
			{
				break;
			}

			case OVERLAPPED_REASON::RECONNECT: // 서버에서 안쓰고 클라에서만 쓰지만 JumpTable 때문에
			{
				break;
			}

			//case OVERLAPPED_REASON::DB_WRITE_TIMEOUT:
			//{
			//	bContinue = true;
			//	break;
			//}

			default:
				__debugbreak();
			}

		} while (0);

		if (bContinue)
		{
			while (!pGameServer->bStopAccept && pGameServer->AcceptPost() != -1);
			continue;
		}

		if (InterlockedDecrement(&pSession->refCnt_) == 0)
			pGameServer->ReleaseSession(pSession);

		while (!pGameServer->bStopAccept && pGameServer->AcceptPost() != -1);
	}
	return 0;
}

//unsigned __stdcall GameServer::AcceptThread(LPVOID arg)
//{
//	SOCKET clientSock;
//	SOCKADDR_IN clientAddr;
//	int addrlen;
//	GameServer* pGameServer = (GameServer*)arg;
//	addrlen = sizeof(clientAddr);
//
//	while (1)
//	{
//		clientSock = accept(pGameServer->hListenSock_, (SOCKADDR*)&clientAddr, &addrlen);
//		InterlockedIncrement((LONG*)&pGameServer->acceptCounter_);
//
//		if (clientSock == INVALID_SOCKET)
//		{
//			DWORD dwErrCode = WSAGetLastError();
//			if (dwErrCode != WSAEINTR && dwErrCode != WSAENOTSOCK)
//			{
//				__debugbreak();
//			}
//			return 0;
//		}
//
//		WCHAR ip[16];
//		InetNtop(AF_INET, &clientAddr.sin_addr, ip, 16);
//		if (!pGameServer->OnConnectionRequest(ip, ntohs(clientAddr.sin_port)))
//		{
//			closesocket(clientSock);
//			continue;
//		}
//
//		// 빈자리가없음 즉 MaxSession
//		auto&& opt = pGameServer->idxStack_.Pop();
//		if (!opt.has_value())
//		{
//			closesocket(clientSock);
//			continue;
//		}
//
//		InterlockedIncrement((LONG*)&pGameServer->lSessionNum_);
//
//		short idx = opt.value();
//		GameSession* pSession = pGameServer->pSessionArr_ + idx;
//		pSession->Init(clientSock, pGameServer->ullIdCounter, idx);
//
//		// 필요할때 쓸일잇으면 쓰라고(주로 블랙 IP로 지정하거나, 로그인서버에서 더미때문에만 씀)
//		CreateIoCompletionPort((HANDLE)pSession->sock_, pGameServer->hcp_, (ULONG_PTR)pSession, 0);
//		++pGameServer->ullIdCounter;
//
//		InterlockedIncrement(&pSession->refCnt_);
//		InterlockedAnd(&pSession->refCnt_, ~GameSession::RELEASE_FLAG);
//
//		pGameServer->OnAccept(pGameServer->GetPlayer(pSession));
//		pGameServer->RecvPost(pSession);
//
//		if (InterlockedDecrement(&pSession->refCnt_) == 0)
//			pGameServer->ReleaseSession(pSession);
//	}
//	return 0;
//}

GameSession* GameServer::GetSession(const void* pPlayer)
{
	short idx = (short)(((ULONG_PTR)pPlayer - (ULONG_PTR)(pPlayerArr_)) / playerSize_);
	return pSessionArr_ + idx;
}

void* GameServer::GetPlayer(const GameSession* pSession)
{
	return (void*)((char*)pPlayerArr_ + playerSize_ * (pSession - pSessionArr_));
}

BOOL GameServer::RecvPost(GameSession* pSession)
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
	int iRecvRet = WSARecv(pSession->sock_, wsa, 2, nullptr, &flags, (LPWSAOVERLAPPED)&(pSession->recvOverlapped), nullptr);
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
		if (dwErrCode == WSAECONNRESET || dwErrCode == WSAECONNABORTED)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}

BOOL GameServer::SendPost(GameSession* pSession)
{
	DWORD dwBufferNum;
	while (1)
	{
		if (pSession->sendPacketQ_.GetSize() <= 0)
			return FALSE;

		// SendFlag 확인 및 변경
		if (InterlockedExchange((LONG*)&pSession->bSendingInProgress_, TRUE) == TRUE)
			return TRUE;

		dwBufferNum = pSession->sendPacketQ_.GetSize();

		if (dwBufferNum <= 0)
			InterlockedExchange((LONG*)&pSession->bSendingInProgress_, FALSE);
		else
			break;
	}

	WSABUF wsa[200];
	DWORD i;
	for (i = 0; i < 200 && i < dwBufferNum; ++i)
	{
#pragma warning(disable : 26815)
		Packet* pPacket = pSession->sendPacketQ_.Dequeue().value();
#pragma warning(default : 26815)
		wsa[i].buf = (char*)pPacket->pBuffer_;
		wsa[i].len = pPacket->GetUsedDataSize() + sizeof(Packet::NetHeader);
		pSession->pSendPacketArr_[i] = pPacket;
	}

	InterlockedExchange(&pSession->lSendBufNum_, i);
	InterlockedAdd(&sendTPS_, i);
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
				CancelIoEx((HANDLE)pSession->sock_, &pSession->sendOverlapped.overlapped);
				return FALSE;
			}
			return TRUE;
		}

		InterlockedDecrement(&(pSession->refCnt_));
		if (dwErrCode == WSAECONNRESET || dwErrCode == WSAECONNABORTED)
			return FALSE;

		LOG(L"Disconnect", ERR, TEXTFILE, L"Client Disconnect By ErrCode : %u", dwErrCode);
		return FALSE;
	}
	return TRUE;
}


int GameServer::AcceptPost()
{
	std::optional<short> opt = idxStack_.Pop();
	if (!opt.has_value())
		return -1;

	short idx = opt.value();
	// Pop한 인덱스에 해당하는 세션 찾기
	GameSession* pSession = pSessionArr_ + idx; 
	// 초기화
	pSession->Init(socket(AF_INET, SOCK_STREAM, 0),
		InterlockedIncrement64((LONG64*)&ullIdCounter) - 1, idx);
	// IOCP에 소켓 등록
	CreateIoCompletionPort((HANDLE)pSession->sock_, hcp_, (ULONG_PTR)pSession, 0); 

	ZeroMemory(&pSession->acceptOverlapped.overlapped, sizeof(WSAOVERLAPPED));
	pSession->acceptOverlapped.why = OVERLAPPED_REASON::ACCEPT;

	DWORD trans;
	// AcceptEx
	if (FALSE == pAcceptExFuncPtr(hListenSock_, pSession->sock_, pSession->recvRB_.GetWriteStartPtr(), 0, 
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &trans, &pSession->acceptOverlapped.overlapped))
	{
		DWORD errCode = WSAGetLastError();
		if (errCode != WSA_IO_PENDING)
		{
			// 연결은 성립되엇지만 Accept 이전에 상대방이 rst를 쏜 경우 WSAECONNRESET
			// 모두 내보내기 위해 리슨 소켓을 닫아버린 경우(해당함수 진입 이후 ShutDown 호출되어 플래그가 바뀌면 여기 진입 가능함 -> 재현완료)
			if (errCode == WSAECONNRESET || (errCode == WSAENOTSOCK && InterlockedXor((LONG*)&bStopAccept, 0) == TRUE))
			{
				idxStack_.Push(idx);
				return 0;
			}
			LOG(L"ERROR", ERR, TEXTFILE, L"AcceptEx Failed ErrCode : %u", WSAGetLastError());
			__debugbreak();
		}
	}
	InterlockedIncrement(&acceptAllocedCnt_);
	return 0;
}

#pragma warning(disable : 26815)

void GameServer::ReleaseSession(GameSession* pSession) 
{
	{
		if (InterlockedCompareExchange(&pSession->refCnt_, GameSession::RELEASE_FLAG | 0, 0) != 0)
			return;

	}
	// Release 될 Session의 Send직렬화 버퍼 정리
	for (LONG i = 0; i < pSession->lSendBufNum_; ++i)
	{
		Packet* pPacket = pSession->pSendPacketArr_[i];
		if (pPacket->DecrementRefCnt() == 0)
		{
			PACKET_FREE(pPacket);
		}
	}

	// Release 될 Session의 Send메시지 큐 정리
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


	pSession->pCurContent->ReleaseSession(pSession);
}

void GameServer::RecvProc(GameSession* pSession, int numberOfBytesTransferred)
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
			Disconnect(pSession->id_);
			return;
		}

		if (pSession->recvRB_.GetUseSize() < sizeof(NetHeader) + header.payloadLen_)
		{
			if (header.payloadLen_ > BUFFER_SIZE)
			{
				Disconnect(pSession->id_);
				return;
			}
			break;
		}
		pSession->recvRB_.MoveOutPos(sizeof(NetHeader));

		Packet* pPacket = PACKET_ALLOC(Net);
		if (header.payloadLen_ > pPacket->bufferSize_ - sizeof(NetHeader)) // 수신 패킷은 최대크기가 정해져잇을것이고 그것도 고려되어 직렬화버퍼의 사이즈가 정해지므로 더 큰 패킷이 오면 끊는다
		{
			Disconnect(pSession->id_);
			PACKET_FREE(pPacket);
			return;
		}

		pSession->recvRB_.Dequeue(pPacket->GetPayloadStartPos<Net>(), header.payloadLen_);
		pPacket->MoveWritePos(header.payloadLen_);
		memcpy(pPacket->pBuffer_, &header, sizeof(Packet::NetHeader));

		// 넷서버에서만 호출되는 함수로 검증 및 디코드후 체크섬 확인
		if (!pPacket->ValidateReceived())
		{
			PACKET_FREE(pPacket);
			Disconnect(pSession->id_);
			return;
		}

		pSession->lastRecvTime = GetTickCount64();
		InterlockedIncrement(&recvTPS_);
		pSession->pCurContent->WorkerHanlePacketAtRecvLoop(pPacket, pSession);
	}
	RecvPost(pSession);
}

void GameServer::SendProc(GameSession* pSession, DWORD dwNumberOfBytesTransferred)
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

void GameServer::AcceptProc(GameSession* pSession)
{
	InterlockedIncrement((LONG*)&acceptCounter_);
	InterlockedDecrement((LONG*)&acceptAllocedCnt_);

	// listen socket의 소켓 속성을 그대로 가져옴(링거 등...)
	if (SOCKET_ERROR == setsockopt(pSession->sock_, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (const char*)&hListenSock_, sizeof(SOCKET)))
	{
		closesocket(pSession->sock_);
		idxStack_.Push((short)(pSession - pSessionArr_));

		DWORD errCode = WSAGetLastError();
		// shtdown시 리슨소켓 닫으면 WSAENOTSOCK이 뜰 수 있음, 
		if ((errCode == WSAENOTSOCK && InterlockedXor((LONG*)&bStopAccept, 0) == TRUE))
			return;

		LOG(L"ERROR", ERR, TEXTFILE, L"setsockopt SO_UPDATE_ACCEPT_CONTEXT At AcceptProc Failed, errCode : %u", errCode);
		__debugbreak();
	}

	SOCKADDR_IN clientAddr;
	ZeroMemory(&clientAddr, sizeof(SOCKADDR_IN));
	int sockAddrLen = sizeof(SOCKADDR_IN);

	if (SOCKET_ERROR == getpeername(pSession->sock_, (SOCKADDR*)&clientAddr, &sockAddrLen))
	{
		closesocket(pSession->sock_);
		idxStack_.Push((short)(pSession - pSessionArr_));

		DWORD errCode = WSAGetLastError();
		LOG(L"ERROR", ERR, TEXTFILE, L"getpeername At AcceptProc Failed, errCode : %u", errCode);
		__debugbreak();
	}

	InetNtop(AF_INET, &clientAddr.sin_addr, pSession->ip_, _countof(GameSession::ip_));
	pSession->port_ = ntohs(clientAddr.sin_port);

	if (!OnConnectionRequest(pSession->ip_, pSession->port_))
	{
		closesocket(pSession->sock_);
		idxStack_.Push((short)(pSession - pSessionArr_));
		return;
	}

	pSession->lastRecvTime = GetTickCount64();
	InterlockedIncrement(&lSessionNum_);
	InterlockedIncrement(&pSession->refCnt_);
	InterlockedAnd(&pSession->refCnt_, ~GameSession::RELEASE_FLAG);

	OnAccept(GetPlayer(pSession));
	RecvPost(pSession);
}

void GameServer::InitialAccept()
{
	while (AcceptPost() != -1);
}

void GameServer::SetEntirePlayerMemory(int playerSize)
{
	playerSize_ = playerSize;
	pPlayerArr_ = malloc(playerSize * maxSession_);
}


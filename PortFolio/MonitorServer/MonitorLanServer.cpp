#include <WinSock2.h>
#include <Common/CommonProtocol.h>
#include <Common/ServerNum.h>
#include <Scheduler/Scheduler.h>
#include "MonitorLanServer.h"
#include "ChatData.h"
#include "EchoData.h"
#include "LoginData.h"
#include "ServerCommonData.h"
#include "MonitorDbThread.h"

ChatData g_chatData;
LoginData g_loginData;
EchoData g_echoData;
ServerCommonData g_common;

struct LanServerIDInfo
{
	ULONGLONG sessionID_;
	SERVERNUM serverNum_;
	LanServerIDInfo()
		:sessionID_{ MAXULONGLONG }, serverNum_{ SERVERNUM::NUM }
	{
	}
};

LONG LoginArr[SERVERNUM::NUM];
LanServerIDInfo LanSessioninfoArr[SERVERNUM::NUM];

#define PROCESS_QUERY_IMPL(Server,elem,dataValue) do{\
	EnterCriticalSection(&Server.cs);\
	Server.elem.total_ += dataValue;\
	++Server.elem.cnt_;\
	Server.elem.min_ = min(Server.elem.min_,dataValue);\
	Server.elem.max_ = max(Server.elem.max_,dataValue);\
	LeaveCriticalSection(&Server.cs);\
}while (0)\


__forceinline void ProcessQueryData(BYTE dataType, int dataValue, int timeStamp)
{
	en_PACKET_SS_MONITOR_DATA_UPDATE_TYPE type = (en_PACKET_SS_MONITOR_DATA_UPDATE_TYPE)dataType;
	switch (type)
	{
	case dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN:
		break;
	case dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU:
	{
		PROCESS_QUERY_IMPL(g_loginData, cpuTime_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM:
	{
		PROCESS_QUERY_IMPL(g_loginData, memAvailableByte_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_LOGIN_SESSION:
	{
		PROCESS_QUERY_IMPL(g_loginData, sessionCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS:
	{
		PROCESS_QUERY_IMPL(g_loginData, loginProcessTps_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL:
	{
		PROCESS_QUERY_IMPL(g_loginData,packetPoolAlloced_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_SERVER_RUN:
		break;
	case dfMONITOR_DATA_TYPE_GAME_SERVER_CPU:
	{
		PROCESS_QUERY_IMPL(g_echoData, cpuTime_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_SERVER_MEM:
	{
		PROCESS_QUERY_IMPL(g_echoData, memAvailableByte_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_SESSION:
	{
		PROCESS_QUERY_IMPL(g_echoData, sessionCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER:
	{
		PROCESS_QUERY_IMPL(g_echoData, authPlayerCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER:
	{
		PROCESS_QUERY_IMPL(g_echoData, gamePlayerCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS:
	{
		PROCESS_QUERY_IMPL(g_echoData, acceptTps_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS:
	{
		PROCESS_QUERY_IMPL(g_echoData, recvTPS_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS:
	{
		PROCESS_QUERY_IMPL(g_echoData, sendTPS_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_DB_WRITE_TPS:
		break;
	case dfMONITOR_DATA_TYPE_GAME_DB_WRITE_MSG:
		break;
	case dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS:
	{
		PROCESS_QUERY_IMPL(g_echoData, authThreadFPS_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS:
	{
		PROCESS_QUERY_IMPL(g_echoData, gameThreadFPS_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_GAME_PACKET_POOL:
	{
		PROCESS_QUERY_IMPL(g_echoData, packetPoolAlloced_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN:
		break;
	case dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU:
	{
		PROCESS_QUERY_IMPL(g_chatData, cpuTime_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM:
	{
		PROCESS_QUERY_IMPL(g_chatData, memAvailableByte_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_SESSION:
	{
		PROCESS_QUERY_IMPL(g_chatData, sessionCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_PLAYER:
	{
		PROCESS_QUERY_IMPL(g_chatData, playerCnt_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS:
	{
		PROCESS_QUERY_IMPL(g_chatData, updateTps_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL:
	{
		PROCESS_QUERY_IMPL(g_chatData, packetPoolAlloced_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL:
	{
		PROCESS_QUERY_IMPL(g_chatData, mQAlloced_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL:
	{
		PROCESS_QUERY_IMPL(g_common, cpuTime_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY:
	{
		PROCESS_QUERY_IMPL(g_common, nonPagedPoolAvaliableBytes_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV:
	{
		PROCESS_QUERY_IMPL(g_common, netWorkRecvPerSec_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND:
	{
		PROCESS_QUERY_IMPL(g_common, netWorkSendPerSec_, dataValue);
		break;
	}
	case dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY:
	{
		PROCESS_QUERY_IMPL(g_common, memAvailable_, dataValue);
		break;
	}
	default:
		__debugbreak();
		break;
	}
}

__forceinline void ServerOnDBWriteReq(SERVERNUM serverType, MonitorLanServer* pServer)
{
	Packet* pPacket = MonitorDbThread::ALLOC();
	(*pPacket) << (WORD)en_MONITOR_DB_JOB::ONOFF << (int)serverType << (int)SERVERSTATE::ON;
	pServer->pDbThread_->ReqDBWriteJob(pPacket);
}

__forceinline void ServerOffDBWriteReq(SERVERNUM serverType, MonitorLanServer* pServer)
{
	Packet* pPacket = MonitorDbThread::ALLOC();
	(*pPacket) << (WORD)en_MONITOR_DB_JOB::ONOFF << (int)serverType << (int)SERVERSTATE::OFF;
	pServer->pDbThread_->ReqDBWriteJob(pPacket);
}

MonitorLanServer::MonitorLanServer(const WCHAR* pIP, const USHORT port, const DWORD IocpWorkerThreadNum, const DWORD CuncurrentThreadNum, const BOOL bZeroCopy, 
	const LONG maxSession, const BOOL bActivateTimeOut, const LONG userTimeOut, const DWORD timeOutCheckInterval, const DWORD DBWriteInterval, const DWORD DBWriteTimeOut)
	:LanServer{ pIP,port,IocpWorkerThreadNum,CuncurrentThreadNum,bZeroCopy,maxSession,bActivateTimeOut,userTimeOut,timeOutCheckInterval }
{
	// DbTimeOut, Db쓰기스레드
	pDbThread_ = new MonitorDbThread;
	pDbThread_->Start();

	// 지정된 시간마다 DB스레드에 모니터링 데이터 Wrtie요청
	pDbRequestTimer_ = new DBRequestTimer{ DBWriteInterval,hcp_,3,pDbThread_ };
}


BOOL MonitorLanServer::Start(MonitorNetServer* pNetServer)
{
	pConsoleMonitor_ = new MonitoringUpdate{ hcp_,1000,3 };
	pConsoleMonitor_->RegisterMonitor(static_cast<const Monitorable*>(this));

	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		ResumeThread(hIOCPWorkerThreadArr_[i]);


	pNetServer_ = pNetServer;
	pNetServer->Start();

	Scheduler::Register_UPDATE(pDbRequestTimer_);
	Scheduler::Register_UPDATE(pConsoleMonitor_);
	Scheduler::Start();
	ResumeThread(hAcceptThread_);
	return TRUE;
}

BOOL MonitorLanServer::OnConnectionRequest(const WCHAR* pIP, const USHORT port)
{
	if (0 != memcmp(pIP, L"127.0.0.1", 16))
	{
		LOG(L"INVALID_CONNECT", SYSTEM, TEXTFILE, L"IP : %s PORT : %d", pIP, port);
		return FALSE;
	}

	return TRUE;
}


void* MonitorLanServer::OnAccept(ULONGLONG id)
{
	return nullptr;
}

void MonitorLanServer::OnRelease(ULONGLONG id)
{
	SERVERNUM num = SERVERNUM::NUM;
	for (int i = 0; i < SERVERNUM::NUM; ++i)
	{
		if (id == LanSessioninfoArr[i].sessionID_)
		{
			num = (SERVERNUM)i;
			break;
		}
	}

	if (num == SERVERNUM::NUM)
	{
		LOG(L"INVALID_CONNECT", SYSTEM, TEXTFILE, L"OnconnectionRequest is did not fullfill his role, ip : %s, port : %d", GetSessionIP(id), GetSessionPort(id));
		return;
	}

	switch (num)
	{
	case CHAT:
	{
		EnterCriticalSection(&g_chatData.cs);
		g_chatData.onoff_ = false;
		g_chatData.Init();
		LeaveCriticalSection(&g_chatData.cs);
		ServerOffDBWriteReq(SERVERNUM::CHAT, this);
		break;
	}
	case LOGIN:
	{
		EnterCriticalSection(&g_loginData.cs);
		g_loginData.onoff_ = false;
		g_loginData.Init();
		LeaveCriticalSection(&g_loginData.cs);
		ServerOffDBWriteReq(SERVERNUM::LOGIN, this);
		break;
	}
	case GAME:
	{
		EnterCriticalSection(&g_echoData.cs);
		g_echoData.onoff_ = false;
		g_echoData.Init();
		LeaveCriticalSection(&g_echoData.cs);
		ServerOffDBWriteReq(SERVERNUM::GAME, this);
		break;
	}
	default:
		__debugbreak();
		break;
	}

	InterlockedDecrement(LoginArr + num);
	if (InterlockedDecrement(&loginServerNum_) == 0)
	{
		EnterCriticalSection(&g_common.cs);
		g_common.Init();
		LeaveCriticalSection(&g_common.cs);
	}
}


void MonitorLanServer::OnRecv(ULONGLONG id, Packet* pPacket)
{
	// 랜서버여서 그냥 믿으려고 햇으나 누군가의 악의적인 공격으로 추가함.
	try
	{
		WORD type;
		BYTE serverNo;
		(*pPacket) >> type >> serverNo;
		if (serverNo >= SERVERNUM::NUM)
		{
			LOG(L"ERROR", SYSTEM, CONSOLE, L"SERVERNUM %d, Invalid Server OnRecv", serverNo);
			LOG(L"ERROR", SYSTEM, TEXTFILE, L"SERVERNUM %d, Invalid Server OnRecv", serverNo);
			Disconnect(id);
			return;
		}

		switch ((en_PACKET_TYPE)type)
		{
		case en_PACKET_SS_MONITOR_LOGIN:
		{
			if (InterlockedIncrement(LoginArr + serverNo) > 1) // Release 되기 직전에 누가 새로들어오면 끊기겟지만 그정도로 정밀성은 필요없다
			{
				InterlockedDecrement(LoginArr + serverNo);
				LOG(L"ERROR", SYSTEM, CONSOLE, L"SERVERNUM %d, Duplicated Login", serverNo);
				LOG(L"ERROR", SYSTEM, TEXTFILE, L"SERVERNUM %d, Duplicated Login", serverNo);
				Disconnect(id);
				break;
			}

			InterlockedIncrement(&loginServerNum_);
			switch ((SERVERNUM)serverNo)
			{
			case CHAT:
			{
				EnterCriticalSection(&g_chatData.cs);
				g_chatData.onoff_ = true;
				LeaveCriticalSection(&g_chatData.cs);
				break;
			}
			case LOGIN:
			{
				EnterCriticalSection(&g_loginData.cs);
				g_loginData.onoff_ = true;
				LeaveCriticalSection(&g_loginData.cs);
				break;
			}
			case GAME:
			{
				EnterCriticalSection(&g_echoData.cs);
				g_echoData.onoff_ = true;
				LeaveCriticalSection(&g_echoData.cs);
				break;
			}
			default:
				// 위에서 걸러졋어야햇는데 안걸러진게 말이안됨
				__debugbreak();
				break;
			}

			LanSessioninfoArr[serverNo].sessionID_ = id;
			LanSessioninfoArr[serverNo].serverNum_ = (SERVERNUM)serverNo;

			SmartPacket sp = PACKET_ALLOC(Lan);
			(*sp) << (WORD)1;
			SendPacket(id, sp);
			ServerOnDBWriteReq((SERVERNUM)serverNo, this);
			break;
		}
		case en_PACKET_SS_MONITOR_DATA_UPDATE:
		{
			BYTE dataType;
			int dataValue;
			int timeStamp;
			(*pPacket) >> dataType >> dataValue >> timeStamp;

			// 로그인시에 서버번호에 맞게 세션아이디가 초기화되엇을것이므로 누군가 로그인하지않고 보내야만 여기걸림
			if (LanSessioninfoArr[serverNo].sessionID_ != id)
			{
				Disconnect(id);
				break;
			}

			pNetServer_->SendToAllClient(serverNo, dataType, dataValue, timeStamp);
			ProcessQueryData(dataType, dataValue, timeStamp);
			break;
		}
		default:
			break;
		}
	}
	catch (int errCode)
	{
		if (errCode == ERR_PACKET_EXTRACT_FAIL)
		{
			Disconnect(id);
		}
		else if (errCode == ERR_PACKET_RESIZE_FAIL)
		{
			// 리사이즈가 일어날 일이없음
			LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
			__debugbreak();
		}
	}

	PACKET_FREE(pPacket);
}

void MonitorLanServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void MonitorLanServer::OnPost(void* order)
{
}

void MonitorLanServer::OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown()
{
	pNetServer_->ShutDown();
	pDbThread_->ReqMonitorWrite();
	pDbThread_->SetShutDown();
	pDbThread_->WaitUntilDbWriteComplete();
}

void MonitorLanServer::OnResourceCleanAtShutDown()
{
	delete pConsoleMonitor_;
	delete pDbRequestTimer_;
	delete pDbThread_;
}

void MonitorLanServer::OnMonitor()
{
	FILETIME ftCreationTime, ftExitTime, ftKernelTime, ftUsertTime;
	FILETIME ftCurTime;
	GetProcessTimes(GetCurrentProcess(), &ftCreationTime, &ftExitTime, &ftKernelTime, &ftUsertTime);
	GetSystemTimeAsFileTime(&ftCurTime);

	ULARGE_INTEGER start, now;
	start.LowPart = ftCreationTime.dwLowDateTime;
	start.HighPart = ftCreationTime.dwHighDateTime;
	now.LowPart = ftCurTime.dwLowDateTime;
	now.HighPart = ftCurTime.dwHighDateTime;


	ULONGLONG ullElapsedSecond = (now.QuadPart - start.QuadPart) / 10000 / 1000;

	ULONGLONG temp = ullElapsedSecond;

	ULONGLONG ullElapsedMin = ullElapsedSecond / 60;
	ullElapsedSecond %= 60;

	ULONGLONG ullElapsedHour = ullElapsedMin / 60;
	ullElapsedMin %= 60;

	ULONGLONG ullElapsedDay = ullElapsedHour / 24;
	ullElapsedHour %= 24;

	LONG64 recvTPS = InterlockedExchange64(&recvTPS_, 0);
	LONG sendTPS = InterlockedExchange(&pNetServer_->sendTPS_, 0);
	
	static int shutDownFlag = 10;
	static int sdfCleanFlag = 0; // 1분넘어가면 초기화

	printf(
		"Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n"
		"Remaining PgDn Key Push To Shut Down : %d\n"
		"LanSession Num : %d\n"
		"Lan Login UserServer Num : %d\n"
		"NetSession Num : %d\n"
		"Net Login MonitorClient Num : %d\n"
		"Lan RecvTPS : %llu\n"
		"Net SendTPS: %d\n"
		"DbWriteThread Pending Count : %d\n\n",
		ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond,
		shutDownFlag,
		lSessionNum_,
		loginServerNum_,
		pNetServer_->lSessionNum_,
		pNetServer_->monitorClientNum_,
		recvTPS,
		sendTPS,
		pDbThread_->dbReqQ_.GetSize()
	);

	++sdfCleanFlag;
	if (sdfCleanFlag == 60)
	{
		shutDownFlag = 10;
		sdfCleanFlag = 0;
	}

	if (GetAsyncKeyState(VK_NEXT) & 0x0001)
	{
		--shutDownFlag;
		if (shutDownFlag == 0)
		{
			printf("Start ShutDown !\n");
			RequestShutDown();
			return;
		}
	}
}


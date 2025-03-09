#include <WinSock2.h>
#include <windows.h>
#include "LoginChatServer.h"
#include "Player.h"
#include "GameServerTimeOut.h"
#include "Job.h"
#include "Sector.h"
#include "RecvLayer.h"
#include "JobProcessLayer.h"
#include "DuplicateLogin.h"
#pragma comment(lib,"Synchronization.lib")
#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")

extern CLockFreeQueue<JOB*> g_jobQ;

enum en_ContentsType
{
	RECV_LAYER,
	JOB_LAYER
};

LoginChatServer::LoginChatServer(WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, LONG maxUser, BYTE packetCode, BYTE packetfixedKey)
	:GameServer{ pIP,port,iocpWorkerNum,cunCurrentThreadNum,bZeroCopy,maxSession,maxUser,sizeof(Player),packetCode,packetfixedKey }, hUpdateThreadWakeEvent_{ CreateEvent(NULL,FALSE,FALSE,NULL) }
{
}

LoginChatServer::~LoginChatServer()
{
}

void LoginChatServer::Start(DWORD tickPerFrame, DWORD timeOutCheckInterval, LONG sessionTimeOut, LONG authUserTimeOut)
{
	DuplicateLogin::Init();
	Player::MAX_PLAYER_NUM = maxPlayer_;

	// Baking
	for (int y = 0; y < NUM_OF_SECTOR_VERTICAL; ++y)
	{
		for (int x = 0; x < NUM_OF_SECTOR_HORIZONTAL; ++x)
		{
			GetSectorAround(x, y, &g_sectorAround[y][x]);
		}
	}

	pRecvLayer_ = new RecvLayer{ this };
	ContentsBase::RegisterContents(RECV_LAYER, pRecvLayer_);
	ContentsBase::SetContentsToFirst(RECV_LAYER);

	for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
		ResumeThread(hIOCPWorkerThreadArr_[i]);

	pTimeOut_ = new GameServerTimeOut{ timeOutCheckInterval,hcp_,3,sessionTimeOut,authUserTimeOut,this };
	pJobProcessLayer_ = new JobProcessLayer{ tickPerFrame,hcp_,3,this };
	pConsoleMonitor_ = new MonitoringUpdate{ hcp_,1000,3 };
	pConsoleMonitor_->RegisterMonitor(static_cast<const Monitorable*>(this));

	Scheduler::Register_UPDATE(pConsoleMonitor_);
	Scheduler::Register_UPDATE(pTimeOut_);
	Scheduler::Register_UPDATE(pJobProcessLayer_);
	Scheduler::Start();
}

BOOL LoginChatServer::OnConnectionRequest(const WCHAR* pIP, const USHORT port)
{
	return TRUE;
}

void* LoginChatServer::OnAccept(void* pPlayer)
{
	((Player*)(pPlayer))->sessionId_ = GetSessionID(pPlayer);
	ContentsBase::FirstEnter(pPlayer);
	return nullptr;
}

void LoginChatServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

#pragma warning(disable : 4302)
#pragma warning(disable : 4311)
void LoginChatServer::OnPost(void* order)
{
	JOB::FlushOrderJobQ((int)order);
}

void LoginChatServer::OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown()
{
}
void LoginChatServer::OnMonitor()
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

	monitor.UpdateCpuTime(nullptr, nullptr);
	monitor.UpdateQueryData();

	ULONGLONG acceptTPS = InterlockedExchange(&acceptCounter_, 0);
	ULONGLONG disconnectTps = InterlockedExchange(&disconnectTPS_, 0);
	ULONGLONG recvTps = InterlockedExchange(&recvTPS_, 0);
	LONG sendTps = InterlockedExchange(&sendTPS_, 0);
	LONG sessionNum = lSessionNum_;
	LONG playerNum = lPlayerNum_;

	acceptTotal_ += acceptTPS;
	RECV_TOTAL_ += recvTps;

	printf(
		"Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n"
		"Packet Pool Alloc Capacity : %d\n"
		"MessageQ Queued By Worker : %d\n"
		"SendQ Pool Capacity : %d\n"
		"Accept TPS: %llu\n"
		"Accept Total : %llu\n"
		"Disconnect TPS: %llu\n"
		"Recv Msg TPS: %llu\n"
		"Send Msg TPS: %d\n"
		"Session Num : %d\n"
		"Player Num : %d\n"
		"RECV TOTAL : %llu\n"
		"RECV_AVR : %.2f\n"
		"----------------------\n"
		"Process Private MBytes : %.2lf\n"
		"Process NP Pool KBytes : %.2lf\n"
		"Memory Available MBytes : %.2lf\n"
		"Machine NP Pool MBytes : %.2lf\n"
		"Processor CPU Time : %.2f\n"
		"Process CPU Time : %.2f\n"
		"SECTOR_ADD_JOB_SIZE : %d\n"
		"SECTOR_REMOVE_JOB_SIZE : %d\n"
		"SECTOR_BROADCAST_JOB_SIZE : %d\n",
		ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond,
		Packet::packetPool_.capacity_,
		g_jobQ.GetSize(),
		pSessionArr_[0].sendPacketQ_.nodePool_.capacity_,
		acceptTPS,
		acceptTotal_,
		disconnectTps,
		recvTps,
		sendTps,
		sessionNum,
		playerNum,
		RECV_TOTAL_,
		RECV_TOTAL_ / (float)temp,
		monitor.GetPPB() / (1024 * 1024),
		monitor.GetPNPB() / 1024,
		monitor.GetAB(),
		monitor.GetNPB() / (1024 * 1024),
		monitor._fProcessorTotal,
		monitor._fProcessTotal,
		SECTOR_ADD_JOB::addJobPool_.capacity_,
		SECTOR_REMOVE_JOB::removeJobPool_.capacity_,
		BROADCAST_JOB::broadCastJobPool_.capacity_
	);
}
#pragma warning(default: 4302)
#pragma warning(default: 4311)

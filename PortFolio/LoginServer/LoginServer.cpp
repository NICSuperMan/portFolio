#include <WinSock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#include "LoginServer.h"
#include "LoginPlayer.h"
#include "Packet.h"
#include "CommonProtocol.h"
#include "LoginServer.h"
#include "LoginContent.h"

#pragma comment(lib,"libmysql.lib")

#pragma warning(disable : 26495)
LoginServer::LoginServer(WCHAR* pIP, USHORT port, DWORD iocpWorkerNum, DWORD cunCurrentThreadNum, BOOL bZeroCopy, LONG maxSession, LONG maxUser, BYTE packetCode, BYTE packetfixedKey, USHORT chatPort, BOOL bLoopBackTest, CMClient* pMonitorLanClient)
    :GameServer{ pIP,port,iocpWorkerNum,cunCurrentThreadNum,bZeroCopy,maxSession,maxUser,sizeof(LoginPlayer),packetCode,packetfixedKey }, ChatServerPort_{ chatPort }, bLoopBackTest_{ bLoopBackTest }, pLanClient_{ pMonitorLanClient }
{
}

LoginServer::~LoginServer()
{
    delete pConsoleMonitor_;
    delete pLoginContent_;
    delete pLanClient_;
}

#pragma warning(default : 26495)

void LoginServer::Start()
{
    // 콘솔 출력 설정
    pConsoleMonitor_ = new MonitoringUpdate{ hcp_,1000,3 };
    Scheduler::Register_UPDATE(pConsoleMonitor_);
    pConsoleMonitor_->RegisterMonitor(this);

    pLoginContent_ = new LoginContent{ this };
    ContentsBase::RegisterContents(0, pLoginContent_);
    ContentsBase::SetContentsToFirst(0);

    // 더미들의 IP주소 구하기
    if (!bLoopBackTest_)
    {
        InetPtonW(AF_INET, L"10.0.1.2", &DummyIP1Point2_.sin_addr);
        InetPtonW(AF_INET, L"10.0.2.2", &DummyIP2Point2_.sin_addr);
    }

    pLanClient_->Start();
    Scheduler::Start();

    for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
        ResumeThread(hIOCPWorkerThreadArr_[i]);
}

BOOL LoginServer::OnConnectionRequest(const WCHAR* pIP, const USHORT port)
{
    return TRUE;
}

void LoginServer::RegisterMonitorLanClient(CMClient* pClient)
{
    pLanClient_ = pClient;
}

void* LoginServer::OnAccept(void* pPlayer)
{
    ((LoginPlayer*)(pPlayer))->sessionID = GetSessionID(pPlayer);
    ContentsBase::FirstEnter(pPlayer);
    return nullptr;
}

void LoginServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void LoginServer::OnPost(void* order)
{
}

void LoginServer::OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown()
{
}


void LoginServer::OnMonitor()
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
    ULONGLONG disconnectTPS = InterlockedExchange(&disconnectTPS_, 0);
    ULONGLONG recvTPS = InterlockedExchange(&recvTPS_, 0);
    LONG sendTPS = InterlockedExchange(&sendTPS_, 0);
    LONG authTPS = InterlockedExchange(&authTPS_, 0);
    LONG sessionNum = InterlockedXor(&lSessionNum_, 0);
    LONG packetPoolUseCount = InterlockedXor(&Packet::packetPool_.AllocSize_, 0);
    acceptTotal_ += acceptTPS;

    double processPrivateMByte = monitor.GetPPB() / (1024 * 1024);

    static int shutDownFlag = 10;
    static int sdfCleanFlag = 0; // 1분넘어가면 초기화

    printf(
        "Elapsed Time : %02lluD-%02lluH-%02lluMin-%02lluSec\n"
        "Remaining PgUp Key Push To Shut Down : %d\n"
        "MonitorServerConnected : %s\n"
        "Packet Pool Alloc Capacity : %d\n"
        "Packet Pool Alloc UseSize: %d\n"
        "Accept TPS: %llu\n"
        "Accept Total : %llu\n"
        "Disconnect TPS: %llu\n"
        "Recv Msg TPS: %llu\n"
        "Send Msg TPS: %d\n"
        "Auth TPS : %d\n"
        "Session Num : %d\n"
        "----------------------\n"
        "Process Private MBytes : %.2lf\n"
        "Process NP Pool KBytes : %.2lf\n"
        "Process CPU Time : %.2f\n\n",
        ullElapsedDay, ullElapsedHour, ullElapsedMin, ullElapsedSecond,
        shutDownFlag,
        (pLanClient_->bLogin_ == TRUE) ? "True" : "False",
        Packet::packetPool_.capacity_ * Bucket<Packet, false>::size,
        packetPoolUseCount,
        acceptTPS,
        acceptTotal_,
        disconnectTPS,
        recvTPS,
        sendTPS,
        authTPS,
        sessionNum,
        processPrivateMByte,
        monitor.GetPNPB() / 1024,
        monitor._fProcessTotal
    );

    ++sdfCleanFlag;
    if (sdfCleanFlag == 60)
    {
        shutDownFlag = 10;
        sdfCleanFlag = 0;
    }

    if (GetAsyncKeyState(VK_PRIOR) & 0x0001)
    {
        --shutDownFlag;
        if (shutDownFlag == 0)
        {
            printf("Start ShutDown !\n");
            RequestShutDown();
            return;
        }
    }

    if (pLanClient_->bLogin_ == FALSE)
        return;

    time_t curTime;
    time(&curTime);
#pragma warning(disable : 4244) // 프로토콜이 4바이트를 받고 상위4바이트는 버려서 별수없음
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SERVER_RUN, 1, curTime);
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU, monitor._fProcessTotal, curTime);
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM, processPrivateMByte, curTime);
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SESSION, sessionNum, curTime);
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS, authTPS, curTime);
    pLanClient_->SendToMonitoringServer(LOGIN, dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL, packetPoolUseCount, curTime);
#pragma warning(default : 4244)
}

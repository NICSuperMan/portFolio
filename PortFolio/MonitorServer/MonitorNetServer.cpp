#include <winsock2.h>
#include <windows.h>
#include <Common/CommonProtocol.h>
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <NetServer/NetSession.h>
#include <Assert.h>
#include <unordered_set>
#include "MonitorNetServer.h"


MonitorNetServer::MonitorNetServer(const WCHAR* pIP, const USHORT port, const DWORD IocpWorkerThreadNum, const DWORD CuncurrentThreadNum, const BOOL bZeroCopy, const LONG maxSession,const LONG maxUser, const BYTE packetCode, const BYTE packetFixedKey, const BOOL bActivateTimeOut, const LONG userTimeOut, const DWORD timeOutCheckInterval)
    :NetServer{ pIP,port,IocpWorkerThreadNum,CuncurrentThreadNum,bZeroCopy,maxSession,packetCode,packetFixedKey,bActivateTimeOut,userTimeOut,timeOutCheckInterval }
{
    InitializeSRWLock(&uMapLock);
}


BOOL MonitorNetServer::Start()
{
    for (DWORD i = 0; i < IOCP_WORKER_THREAD_NUM_; ++i)
        ResumeThread(hIOCPWorkerThreadArr_[i]);

    ResumeThread(hAcceptThread_);
    return TRUE;
}

BOOL MonitorNetServer::OnConnectionRequest(const WCHAR* pIP, USHORT port)
{
    return TRUE;
}

void* MonitorNetServer::OnAccept(ULONGLONG id)
{
    return nullptr;
}

void MonitorNetServer::OnRelease(ULONGLONG id)
{
    PostQueuedCompletionStatus(hcp_, 4, (ULONG_PTR)id, (LPOVERLAPPED)&OnPostOverlapped);
}

void MonitorNetServer::OnRecv(ULONGLONG id, Packet* pPacket)
{
    try
    { 
		WORD type;
		(*pPacket) >> type;
		switch ((en_PACKET_TYPE)type)
		{
		case en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN:
		{
            if (!pPacket->GetPointer(32))
            {
                Disconnect(id);
                break;
            }
			NetSession* pSession = pSessionArr_ + NetSession::GET_SESSION_INDEX(id);
			// 모니터링 클라 자료구조에 추가
			AcquireSRWLockExclusive(&uMapLock);
			uMap.insert(id);
			InterlockedIncrement(&monitorClientNum_);
			ReleaseSRWLockExclusive(&uMapLock);

			// 그사이에 Release되엇으면 SendPacket이 실패함
			SmartPacket pSendPacket = PACKET_ALLOC(Net);
			(*pSendPacket) << (WORD)en_PACKET_CS_MONITOR_TOOL_RES_LOGIN << (BYTE)dfMONITOR_TOOL_LOGIN_OK;
			SendPacket(id, pSendPacket);
			break;
		}
		default:
            Disconnect(id);
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
            // 해당 서버는 리사이즈가 일어날 일이없음
            LOG(L"RESIZE", ERR, TEXTFILE, L"Resize Fail ShutDown Server");
            __debugbreak();
        }
    }
  
    PACKET_FREE(pPacket);
}

void MonitorNetServer::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void MonitorNetServer::OnPost(void* order)
{
    ULONGLONG id = (ULONGLONG)order;
    OnRelease_IMPL(id);
}

void MonitorNetServer::OnLastTaskBeforeAllWorkerThreadEndBeforeShutDown()
{
}

void MonitorNetServer::OnResourceCleanAtShutDown()
{
}

void MonitorNetServer::SendToAllClient(BYTE serverNo, BYTE dataType, int dataValue, int timeStamp)
{
    SmartPacket sp = PACKET_ALLOC(Net);
    (*sp) << (WORD)en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE << serverNo << dataType << dataValue << timeStamp;
    AcquireSRWLockShared(&uMapLock);
    for (ULONGLONG id: uMap)
        SendPacket(id, sp);
    ReleaseSRWLockShared(&uMapLock);
}


void MonitorNetServer::OnRelease_IMPL(ULONGLONG id)
{
    AcquireSRWLockExclusive(&uMapLock);
    size_t ret = uMap.erase(id);
    ReleaseSRWLockExclusive(&uMapLock);
    if (ret != 0)
    {
        InterlockedDecrement(&monitorClientNum_);
    }
    else
    {
        LOG(L"May Be Attack", ERR, TEXTFILE, L"Non Login Client Release IP : %s, Port : %d", GetIP(id), GetPort(id));
    }
}

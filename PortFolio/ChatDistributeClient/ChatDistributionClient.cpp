#include <Winsock2.h>
#include <DataStructure/CLockFreeQueue.h>
#include <Scheduler/Scheduler.h>
#include <Common/CommonProtocol.h>
#include <Parser.h>
#include "ChatDistributionClient.h"
#include "DistributionClientConstant.h"
#include "HeartBeatUpdate.h"


#define WM_REQUEST_INVALIDATE_RECV (WM_USER + 1)

extern HWND g_hWnd;
extern BOOL g_bFirstRecvd;


ChatDistributionClient::ChatDistributionClient(BOOL bAutoReconnect, LONG autoReconnectCnt, LONG autoReconnectInterval, BOOL bUseMemberSockAddrIn, WCHAR* pIP, USHORT port, DWORD iocpWorkerThreadNum, DWORD cunCurrentThreadNum, LONG maxSession, BYTE packetCode, BYTE packetFixedKey, DWORD reqInterval, DWORD alertNum)
	:NetClient{ bAutoReconnect,autoReconnectCnt,autoReconnectInterval,bUseMemberSockAddrIn,pIP,port,iocpWorkerThreadNum,cunCurrentThreadNum,maxSession,packetCode,packetFixedKey }, reqInterval_{ reqInterval }, alertNum_{ alertNum }, ClientSessionID_{ ULONGLONG() }, RequestInfo_{ nullptr }
{
}


BOOL ChatDistributionClient::Start()
{
	Scheduler::Init();
	for (DWORD i = 0; i < IOCP_ACTIVE_THREAD_NUM_; ++i)
	{
		ResumeThread(hIOCPWorkerThreadArr_[i]);
	}

	InitialConnect(&sockAddr_);
	return TRUE;
}

ChatDistributionClient::~ChatDistributionClient()
{
}

void ChatDistributionClient::OnRecv(ULONGLONG id, SmartPacket& sp)
{
	// WPARAM은 86,64에서 전부 포인터 크기임
	sp->IncreaseRefCnt();
	PostMessage(g_hWnd, WM_REQUEST_INVALIDATE_RECV, (WPARAM)sp.GetPacket(), 0);
}

void ChatDistributionClient::OnError(ULONGLONG id, int errorType, Packet* pRcvdPacket)
{
}

void ChatDistributionClient::OnConnect(ULONGLONG id)
{
	ClientSessionID_ = id;
	SmartPacket sp = PACKET_ALLOC(Net);
	*sp << (WORD)en_PACKET_CS_CHAT_MONITOR_CLIENT_LOGIN << MONITOR_CLINET_ACCOUNT_NO;
	SendPacket(id, sp);

	RequestInfo_ = new RequestSectorInfo{ (DWORD)reqInterval_,hcp_,3,this };
	Scheduler::Register_UPDATE(RequestInfo_);
	Scheduler::Start();
}

void ChatDistributionClient::OnRelease(ULONGLONG id)
{
	delete RequestInfo_;
	PostMessage(g_hWnd, WM_DESTROY, 0, 0);
}

void ChatDistributionClient::OnConnectFailed(ULONGLONG id)
{
	LOG(L"CONNECT_FAIL", ERR, TEXTFILE, L"CONNECT_FAILED  Errcode :%u", WSAGetLastError());
	PostMessage(g_hWnd, WM_DESTROY, 0, 0);
}

// 안씀
void ChatDistributionClient::OnAutoResetAllFailed()
{
}

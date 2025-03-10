#include <WinSock2.h>
#include <Windows.h>
#include "RingBuffer.h"
#include "Packet.h"
#include "CLockFreeQueue.h"
#include "MYOVERLAPPED.h"
#include "NetClientSession.h"

BOOL NetClientSession::Init(ULONGLONG ullClientID, SHORT shIdx)
{
	bSendingInProgress_ = FALSE;
	InterlockedExchange(&id_, ((ullClientID << 16) ^ shIdx));
	lastRecvTime = GetTickCount64();
	bDisconnectCalled_ = FALSE;
	lSendBufNum_ = 0;
	recvRB_.ClearBuffer();
    return TRUE;
}

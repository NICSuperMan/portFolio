#include <WinSock2.h>
#include <Windows.h>
#include <SerialLizeBuffer_AND_RingBuffer/RingBuffer.h>
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <DataStructure/CLockFreeQueue.h>
#include <Common/MYOVERLAPPED.h>
#include <NetClient/NetClientSession.h>

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

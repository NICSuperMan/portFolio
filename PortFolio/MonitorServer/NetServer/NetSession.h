#pragma once
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <SerialLizeBuffer_AND_RingBuffer/RingBuffer.h>
#include <Common/MYOVERLAPPED.h>

class Packet;

struct NetSession
{
	static constexpr LONG RELEASE_FLAG = 0x80000000;
	SOCKET sock_;
	ULONGLONG id_;
	ULONGLONG lastRecvTime;
	LONG lSendBufNum_;
	BOOL bDisconnectCalled_;
	MYOVERLAPPED recvOverlapped;
	MYOVERLAPPED sendOverlapped;
	LONG refCnt_;
	CLockFreeQueue<Packet*> sendPacketQ_;
	BOOL bSendingInProgress_;
	BOOL bSendingAtWorker_;
	Packet* pSendPacketArr_[50];
	RingBuffer recvRB_;
	WCHAR ip_[16];
	USHORT port_;
	BOOL Init(SOCKET clientSock, ULONGLONG ullClientID, SHORT shIdx);

	NetSession()
		:refCnt_{ NetSession::RELEASE_FLAG | 0 }
	{}

	inline static short GET_SESSION_INDEX(ULONGLONG id)
	{
		return id & 0xFFFF;
	}
};

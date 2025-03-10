#pragma once
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include <SerialLizeBuffer_AND_RingBuffer/RingBuffer.h>
#include <Common/MYOVERLAPPED.h>
#include <DataStructure/CLinkedList.h>

class Packet;
class ContentsBase;

struct GameSession
{
	static constexpr LONG RELEASE_FLAG = 0x80000000;
	LINKED_NODE node{ offsetof(GameSession,node) };
	ContentsBase* pCurContent;
	int ReservedNextContent; // SerialContent���� RegisterLeave���� ������������ ���� ������ ������ ��ȣ�� �����Ҷ� ����
	int serialIdx;
	CLockFreeQueue<Packet*> recvMsgQ_;
	SOCKET sock_;
	ULONGLONG id_;
	ULONGLONG lastRecvTime;
	LONG lSendBufNum_;
	BOOL bDisconnectCalled_;
	MYOVERLAPPED recvOverlapped;
	MYOVERLAPPED sendOverlapped;
	LONG refCnt_;
	BOOL bLogin_;
	MYOVERLAPPED acceptOverlapped;
	CLockFreeQueue<Packet*> sendPacketQ_;
	BOOL bSendingInProgress_;
	Packet* pSendPacketArr_[200];
	RingBuffer recvRB_;
	WCHAR ip_[16];
	USHORT port_;
	BOOL Init(SOCKET clientSock, ULONGLONG counter , SHORT idx);

	GameSession()
		:refCnt_{ GameSession::RELEASE_FLAG | 0 }
	{}

	__forceinline static short GET_SESSION_INDEX(ULONGLONG id)
	{
		return id & 0xFFFF;
	}

};

using CSession = const GameSession;

#pragma once
#include <SerialLizeBuffer_AND_RingBuffer/Packet.h>
#include "CurrentServerType.h"

enum class JOBTYPE
{
	CONNECT_SUCCESS,
	RECV_MESSAGE,
	ON_RELEASE,
	CONNECT_FAIL,
	ALL_RECONNECT_FAIL,
};

struct RecvJob
{
	const SERVERTYPE serverType_;
	const JOBTYPE type_;
	const ULONGLONG sessionId_;
	Packet* pPacket_;

	RecvJob(SERVERTYPE serverType, JOBTYPE type, ULONGLONG sessionId, Packet* pPacket = nullptr)
		:serverType_{ serverType }, type_{ type }, sessionId_{ sessionId }, pPacket_{ pPacket }
	{}
};

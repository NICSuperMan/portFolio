#pragma once
#include "MonitorData.h"

struct ServerCommonData
{
	MonitorData cpuTime_;
	MonitorData nonPagedPoolAvaliableBytes_;
	MonitorData memAvailable_;
	MonitorData netWorkRecvPerSec_;
	MonitorData netWorkSendPerSec_;
	CRITICAL_SECTION cs;
	ServerCommonData()
	{
		InitializeCriticalSection(&cs);
	}

	ServerCommonData& operator=(const ServerCommonData& rhs)
	{
		if (this == &rhs)
			return *this;

		cpuTime_ = rhs.cpuTime_;
		nonPagedPoolAvaliableBytes_ = rhs.nonPagedPoolAvaliableBytes_;
		memAvailable_ = rhs.memAvailable_;
		netWorkRecvPerSec_ = rhs.netWorkRecvPerSec_;
		netWorkSendPerSec_ = rhs.netWorkSendPerSec_;
		return *this;
	}

	__forceinline void Init()
	{
		new(&cpuTime_)MonitorData{};
		new(&nonPagedPoolAvaliableBytes_)MonitorData{};
		new(&memAvailable_)MonitorData{};
		new(&netWorkRecvPerSec_)MonitorData{};
		new(&netWorkSendPerSec_)MonitorData{};
	}
};

#pragma once
#include <new>
#include "MonitorData.h"


struct ChatData
{
	bool onoff_ = false;			
	MonitorData cpuTime_;			
	MonitorData memAvailableByte_;			 
	MonitorData sessionCnt_;		
	MonitorData playerCnt_;		
	MonitorData updateTps_;		
	MonitorData packetPoolAlloced_;		
	MonitorData mQAlloced_;			
	CRITICAL_SECTION cs;
	ChatData()
	{
		InitializeCriticalSection(&cs);
	}

	ChatData& operator=(const ChatData& rhs)
	{
		if (this == &rhs)
			return *this;

		onoff_ = rhs.onoff_;
		cpuTime_ = rhs.cpuTime_;
		memAvailableByte_ = rhs.memAvailableByte_;
		sessionCnt_ = rhs.sessionCnt_;
		playerCnt_ = rhs.playerCnt_;
		updateTps_ = rhs.updateTps_;
		packetPoolAlloced_ = rhs.packetPoolAlloced_;
		mQAlloced_ = rhs.mQAlloced_;
		return *this;
	}

	__forceinline void Init()
	{
		new(&cpuTime_)MonitorData{};
		new(&memAvailableByte_)MonitorData{};
		new(&sessionCnt_)MonitorData{};
		new(&playerCnt_)MonitorData{};
		new(&updateTps_)MonitorData{};
		new(&packetPoolAlloced_)MonitorData{};
		new(&mQAlloced_)MonitorData{};
	}
};

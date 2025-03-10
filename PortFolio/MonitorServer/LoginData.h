#pragma once
#include "MonitorData.h"

struct LoginData 
{
	bool onoff_ = false;			// 실행 여부
	MonitorData cpuTime_;			// CPU 사용률
	MonitorData memAvailableByte_;			// 메모리 사용 MByte
	MonitorData sessionCnt_;		// Session 수
	MonitorData loginProcessTps_;			// 초당 로그인 처리 수
	MonitorData packetPoolAlloced_;		// Packet Pool 사용량
	CRITICAL_SECTION cs;
	LoginData()
	{
		InitializeCriticalSection(&cs);
	}

	LoginData& operator=(const LoginData& rhs)
	{
		if (this == &rhs)
			return *this;

		onoff_ = rhs.onoff_;
		cpuTime_ = rhs.cpuTime_;
		memAvailableByte_ = rhs.memAvailableByte_;
		sessionCnt_ = rhs.sessionCnt_;
		loginProcessTps_ = rhs.loginProcessTps_;
		packetPoolAlloced_ = rhs.packetPoolAlloced_;
		return *this;
	}
	__forceinline void Init()
	{
		new(&cpuTime_)MonitorData{};
		new(&memAvailableByte_)MonitorData{};
		new(&sessionCnt_)MonitorData{};
		new(&loginProcessTps_)MonitorData{};
		new(&packetPoolAlloced_)MonitorData{};
	}
};

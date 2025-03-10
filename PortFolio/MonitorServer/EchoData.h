#pragma once
#include "MonitorData.h"

struct EchoData
{
	bool onoff_ = false;
	MonitorData cpuTime_;
	MonitorData memAvailableByte_;
	MonitorData sessionCnt_;
	MonitorData authPlayerCnt_;
	MonitorData gamePlayerCnt_;
	MonitorData acceptTps_;
	MonitorData recvTPS_;
	MonitorData sendTPS_;
	MonitorData authThreadFPS_;
	MonitorData gameThreadFPS_;
	MonitorData packetPoolAlloced_;
	CRITICAL_SECTION cs;
	EchoData()
	{
		InitializeCriticalSection(&cs);
	}

	EchoData& operator=(const EchoData& rhs)
	{
		if (this == &rhs)
			return *this;

		onoff_ = rhs.onoff_;
		cpuTime_ = rhs.cpuTime_;
		memAvailableByte_ = rhs.memAvailableByte_;
		sessionCnt_ = rhs.sessionCnt_;
		authPlayerCnt_ = rhs.authPlayerCnt_;
		gamePlayerCnt_ = rhs.gamePlayerCnt_;
		acceptTps_ = rhs.acceptTps_;
		recvTPS_ = rhs.recvTPS_;
		sendTPS_ = rhs.sendTPS_;
		authThreadFPS_ = rhs.authThreadFPS_;
		gameThreadFPS_ = rhs.gameThreadFPS_;
		packetPoolAlloced_ = rhs.packetPoolAlloced_;
		return *this;
	}

	__forceinline void Init()
	{
		new(&cpuTime_)MonitorData{};
		new(&memAvailableByte_)MonitorData{};
		new(&sessionCnt_)MonitorData{};
		new(&authPlayerCnt_)MonitorData{};
		new(&gamePlayerCnt_)MonitorData{};
		new(&acceptTps_)MonitorData{};
		new(&recvTPS_)MonitorData{};
		new(&sendTPS_)MonitorData{};
		new(&authThreadFPS_)MonitorData{};
		new(&gameThreadFPS_)MonitorData{};
		new(&packetPoolAlloced_)MonitorData{};
	}
};

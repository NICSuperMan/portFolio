#pragma once
#include <windows.h>
#include <stdio.h>
#include "CLockFreeQueue.h"
#include "Monitorable.h"

// Timer스레드가 PQCS를 쏘기위한 스케줄링 정보를 담는 구조체
struct ScheduleRsc
{
	DWORD firstTimeCheck_;
	DWORD oldFrameTick_;
	DWORD timeStamp_;

	__forceinline void Init(DWORD timeToInit)
	{
		firstTimeCheck_ = timeToInit;
	}

	DWORD UpdateAndGetTimeToSleep(DWORD timeStamp, DWORD tickPerFrame)
	{
		timeStamp_ = timeStamp;
		oldFrameTick_ = timeStamp_ - ((timeStamp_ - firstTimeCheck_) % tickPerFrame);
		return tickPerFrame - (timeStamp_ - oldFrameTick_);
	}
};

namespace Scheduler
{
	unsigned __stdcall threadFunc(void* pParam);
}

struct UpdatePQCSInfo;

class UpdateBase
{
public:
	UpdateBase(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit);
	virtual ~UpdateBase();
	void Update();
protected:
	virtual void Update_IMPL() = 0;
private:
	void firstTimeInit();
	bool bFirst_ = false;
	LONG singleThreadGate_ = 0;
	DWORD timeStamp_ = 0;
	DWORD oldFrameTick_ = 0;
	DWORD firstTimeCheck_ = 0;
	const DWORD TICK_PER_FRAME_;
public:
	LONG fps_ = 0;
private:
	LONG unProcessedPqcsCnt_;
	const LONG AllowedUnProcessedPqcsLimit = 0;
private:
	alignas(64) ScheduleRsc scdr;
protected:
	const HANDLE hcp_;
private:
	friend struct UpdatePQCSInfo;
	friend unsigned __stdcall Scheduler::threadFunc(void* pParam);
};


class MonitoringUpdate : public UpdateBase 
{
private:
	static constexpr int len = 14;
	Monitorable* pArr_[len];
	int curNum_ = 0;
	virtual void Update_IMPL() override;
public:
	MonitoringUpdate(HANDLE hCompletionPort, DWORD tickPerFrame, LONG pqcsLimit);
	void RegisterMonitor(const Monitorable* pTargetToMonitor);
};

struct UpdatePQCSInfo
{
	HANDLE hTerminateEvent_;
	static constexpr int len = 15;
	UpdateBase* pUpdateArr_[len];
	int currentNum_ = 0;
#pragma warning(disable : 26495)
	UpdatePQCSInfo()
	{
		hTerminateEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (hTerminateEvent_ == NULL)
		{
			DWORD dwErrCode = GetLastError();
			__debugbreak();
		}
	}
#pragma warning(default: 26495)

	void firstTimeInit(DWORD firstTime)
	{
		for (int i = 0; i < currentNum_; ++i)
			pUpdateArr_[i]->scdr.Init(firstTime);
	}
};


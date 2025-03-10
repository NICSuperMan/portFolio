#pragma once
#include "UpdateBase.h"
#include "MYOVERLAPPED.h"

namespace Scheduler
{
	void Register_UPDATE(UpdateBase* pUpdate);
	void Init();
	void Release_SchedulerThread();
	void Start();
	const MYOVERLAPPED* GetUpdateOverlapped();
}

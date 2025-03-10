#pragma once
#include <Scheduler/UpdateBase.h>
#include "LanServer.h"
class LanServerTimeOut : public UpdateBase
{
public:
	LanServerTimeOut(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LanServer* pLanServer);
	virtual ~LanServerTimeOut();
	virtual void Update_IMPL() override;
private:
	LanServer* pLanServer_;
};

#pragma once
#include <Scheduler/UpdateBase.h>
//#include <NetClient/NetClient.h>
#include "ChatDistributionClient.h"
class RequestSectorInfo : public UpdateBase
{
public:
	RequestSectorInfo(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, ChatDistributionClient* pClient);
	~RequestSectorInfo();

	virtual void Update_IMPL() override;
	ChatDistributionClient* pClient_;
};

#pragma once
#include <Scheduler/UpdateBase.h>
class LoginChatServer;
class JobProcessLayer : public UpdateBase
{
public:
	JobProcessLayer(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LoginChatServer* pChatServer);
	virtual ~JobProcessLayer();

	virtual void Update_IMPL() override;
	LoginChatServer* pChatServer_;
};

#pragma once
#include "UpdateBase.h"

class GameServer;
class GameServerTimeOut : public UpdateBase
{
public:
	GameServerTimeOut(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LONG sessionTimeOut, LONG authUserTimeOut, GameServer* pGameServer);
	virtual ~GameServerTimeOut();
	virtual void Update_IMPL() override;
private:
	GameServer* pGameServer_;
	const LONG sessionTimeOut_;
	const LONG authUserTimeOut_;
};

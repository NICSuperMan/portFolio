#pragma once
#include "ParallelContent.h"

class LoginContent : public ParallelContent
{
public:
	LoginContent(LoginServer* pLoginServer);
	~LoginContent();

	// ContentsBase overridng 
	virtual void OnEnter(void* pPlayer) override;
	virtual void OnLeave(void* pPlayer) override;
	virtual void OnRecv(Packet* pPacket, void* pPlayer) override;
	LoginServer* pLoginServer_;
};

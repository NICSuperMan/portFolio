#pragma once
#include <windows.h>
#include "CLockFreeQueue.h"
#include "CTlsObjectPool.h"
#include "Packet.h"

struct MYOVERLAPPED;

class DBWriteThread
{
public:
	__forceinline static Packet* ALLOC()
	{
		Packet* pRet = dbReqPool_.Alloc();
		pRet->Clear<Lan>();
		return pRet;
	}

	DBWriteThread();
	void Start();
	void ReqDBWriteJob(Packet* pPacket);
	void SetShutDown();
	virtual void OnWrite(Packet* pPacket) = 0;
	CLockFreeQueue<Packet*> dbReqQ_;
	void WaitUntilDbWriteComplete();
private:
	BOOL bShutDown_ = FALSE;
	HANDLE hThread_ = nullptr;
	static inline LONG compareSize_ = 0; // For WaitOnAddress, WaitByAddressSingle
	static inline CTlsObjectPool<Packet, false> dbReqPool_;
	friend unsigned _stdcall DBThreadFunc(void* pParam);
};


#include <process.h>
#include "DBWriteThread.h"
#pragma comment(lib,"Synchronization.lib")
unsigned _stdcall DBThreadFunc(void* pParam)
{
	DBWriteThread* pDBBase = (DBWriteThread*)(pParam);
	while (!pDBBase->bShutDown_)
	{
		while (1)
		{
			auto&& opt = pDBBase->dbReqQ_.Dequeue();
			if (!opt.has_value())
				break;

			Packet* pPacket = opt.value();
			pDBBase->OnWrite(pPacket);
			pDBBase->dbReqPool_.Free(pPacket);
		}
		WaitOnAddress(&pDBBase->dbReqQ_.num_, (PVOID)&DBWriteThread::compareSize_, sizeof(LONG), INFINITE);
	}

	// 종료 전 DB에 남은거 전부 처리하고 나가기
	while (1)
	{
		auto&& opt = pDBBase->dbReqQ_.Dequeue();
		if (!opt.has_value())
			break;

		Packet* pPacket = opt.value();
		pDBBase->OnWrite(pPacket);
		pDBBase->dbReqPool_.Free(pPacket);
	}
	return 0;
}

DBWriteThread::DBWriteThread()
{}

void DBWriteThread::Start()
{
	hThread_ = (HANDLE)_beginthreadex(nullptr, 0, DBThreadFunc, this, 0, nullptr);
	if (hThread_ == INVALID_HANDLE_VALUE)
	{
		LOG(L"DBERROR", ERR, TEXTFILE, L"DBThread Create Failed,ErrCode : %u", GetLastError());
		__debugbreak();
	}
}

void DBWriteThread::ReqDBWriteJob(Packet* pPacket)
{
	dbReqQ_.Enqueue(pPacket);
	WakeByAddressSingle((PVOID)&dbReqQ_.num_);
}

void DBWriteThread::SetShutDown()
{
	InterlockedExchange((LONG*)&bShutDown_, TRUE);
	WakeByAddressSingle((PVOID)&dbReqQ_.num_);
}

void DBWriteThread::WaitUntilDbWriteComplete()
{
	WaitForSingleObject(hThread_, INFINITE);
}


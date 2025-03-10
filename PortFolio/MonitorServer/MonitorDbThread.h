#pragma once
#include <MySqlUtil/DBWriteThread.h>
#include <Scheduler/UpdateBase.h>
enum en_MONITOR_DB_JOB
{
	ONOFF,
	MONITORWRITE
};

enum SERVERSTATE
{
	ON,
	OFF
};


class MonitorDbThread : public DBWriteThread
{
public:
	MonitorDbThread();
	void ProcessOnoff(Packet* pPacket);
	void ProcessMonitorWrite();
	virtual void OnWrite(Packet* pPacket) override;
	void ReqMonitorWrite();
};

class DBRequestTimer : public UpdateBase
{
public:
	DBRequestTimer(DWORD requestInterval, HANDLE hCompletionPort, LONG timeOutPqcsLimit, MonitorDbThread* pMonitorDbThread);
	virtual void Update_IMPL() override;
	MonitorDbThread* pMonitorDbThread_;
};

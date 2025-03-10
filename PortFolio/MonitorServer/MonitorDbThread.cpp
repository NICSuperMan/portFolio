#include <Common/CommonProtocol.h>
#include <MySqlUtil/QueryFactory.h>
#include "MonitorDbThread.h"
#include "EchoData.h"
#include "ChatData.h"
#include "LoginData.h"
#include "ServerCommonData.h"
#include "DBWriteType.h"

extern ChatData g_chatData;
extern EchoData g_echoData;
extern LoginData g_loginData;
extern ServerCommonData g_common;

MonitorDbThread::MonitorDbThread()
{
}

void MonitorDbThread::ProcessOnoff(Packet* pPacket)
{
	tm curTime;
	__time64_t temp;
	_time64(&temp);
	_localtime64_s(&curTime, &temp);

	char tableArr[MAX_PATH];
	sprintf_s(tableArr, MAX_PATH, "logdb.onoff_%d%d", curTime.tm_year + 1900, curTime.tm_mon + 1);

	int serverNum;
	int onOffState;
	(*pPacket) >> serverNum >> onOffState;
	while (1)
	{
		QueryFactory::GetInstance()->MAKE_QUERY("INSERT INTO %s (logtime, serverno, onOffState) VALUES (now(), %d, %d)", tableArr, serverNum, onOffState);
		int queryRet = QueryFactory::GetInstance()->ExcuteWriteQuery();

		if (queryRet != 0)
		{
			if (queryRet == 1146) // 테이블 없음(날짜 넘어감)
			{
				QueryFactory::GetInstance()->MAKE_QUERY("CREATE TABLE %s like logdb.onoff", tableArr);
				QueryFactory::GetInstance()->ExcuteWriteQuery();
			}
			else
				__debugbreak();
			continue;
		}
		else
			break;
	}
}

void MakeMonitorQuery(MonitorData* pMD, LPCSTR pTableName, int serverNo, en_PACKET_SS_MONITOR_DATA_UPDATE_TYPE queryType)
{
	while (1)
	{
		if (pMD->cnt_ == 0)
		{
			QueryFactory::GetInstance()->
				MAKE_QUERY("INSERT INTO %s (logtime, serverno, type, avg, min, max) VALUES (now(), %d, %d, %d, %d, %d)", pTableName, serverNo, queryType
					,0, 0, 0);
		}
		else
		{
			QueryFactory::GetInstance()->
				MAKE_QUERY("INSERT INTO %s (logtime, serverno, type, avg, min, max) VALUES (now(), %d, %d, %d, %d, %d)", pTableName, serverNo, queryType
					, (int)(pMD->total_ / (float)pMD->cnt_), pMD->min_, pMD->max_);
		}

		int queryRet = QueryFactory::GetInstance()->ExcuteWriteQuery();

		if (queryRet != 0)
		{
			if (queryRet == 1146) // 테이블 없음(날짜 넘어감)
			{
				QueryFactory::GetInstance()->MAKE_QUERY("CREATE TABLE %s like logdb.monitorlog", pTableName);
				QueryFactory::GetInstance()->ExcuteWriteQuery();
				continue;
			}
			else
				__debugbreak();
		}
		else
			return;
	}
}

void MonitorDbThread::ProcessMonitorWrite()
{
	ChatData chatTemp;
	LoginData loginTemp;
	EchoData echoTemp;
	ServerCommonData hardWareTemp;

	EnterCriticalSection(&g_chatData.cs);
	chatTemp = g_chatData;
	g_chatData.Init();
	LeaveCriticalSection(&g_chatData.cs);

	EnterCriticalSection(&g_loginData.cs);
	loginTemp = g_loginData;
	g_loginData.Init();
	LeaveCriticalSection(&g_loginData.cs);

	EnterCriticalSection(&g_echoData.cs);
	echoTemp = g_echoData;
	g_echoData.Init();
	LeaveCriticalSection(&g_echoData.cs);

	EnterCriticalSection(&g_common.cs);
	hardWareTemp = g_common;
	g_common.Init();
	LeaveCriticalSection(&g_common.cs);

	tm curTime;
	__time64_t temp;
	_time64(&temp);
	_localtime64_s(&curTime, &temp);

	char tableArr[MAX_PATH];
	sprintf_s(tableArr, MAX_PATH, "logdb.monitorlog_%d%d", curTime.tm_year + 1900, curTime.tm_mon + 1);
	
	int Cnt = 0;
	if (chatTemp.onoff_)
	{
		MakeMonitorQuery(&chatTemp.cpuTime_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU);
		MakeMonitorQuery(&chatTemp.memAvailableByte_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM);
		MakeMonitorQuery(&chatTemp.sessionCnt_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_SESSION);
		MakeMonitorQuery(&chatTemp.playerCnt_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_PLAYER);
		MakeMonitorQuery(&chatTemp.updateTps_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS);
		MakeMonitorQuery(&chatTemp.packetPoolAlloced_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL);
		MakeMonitorQuery(&chatTemp.mQAlloced_, tableArr, (int)DBWriteType::CHAT, dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL);
		++Cnt;
	}

	if (loginTemp.onoff_)
	{
		MakeMonitorQuery(&loginTemp.cpuTime_, tableArr, (int)DBWriteType::LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SERVER_CPU);
		MakeMonitorQuery(&loginTemp.memAvailableByte_, tableArr, (int)DBWriteType::LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SERVER_MEM);
		MakeMonitorQuery(&loginTemp.sessionCnt_, tableArr, (int)DBWriteType::LOGIN, dfMONITOR_DATA_TYPE_LOGIN_SESSION);
		MakeMonitorQuery(&loginTemp.loginProcessTps_, tableArr, (int)DBWriteType::LOGIN, dfMONITOR_DATA_TYPE_LOGIN_AUTH_TPS);
		MakeMonitorQuery(&loginTemp.packetPoolAlloced_, tableArr, (int)DBWriteType::LOGIN, dfMONITOR_DATA_TYPE_LOGIN_PACKET_POOL);
		++Cnt;
	}

	if (echoTemp.onoff_)
	{
		MakeMonitorQuery(&echoTemp.cpuTime_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_SERVER_CPU);
		MakeMonitorQuery(&echoTemp.memAvailableByte_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_SERVER_MEM);
		MakeMonitorQuery(&echoTemp.sessionCnt_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_SESSION);
		MakeMonitorQuery(&echoTemp.authPlayerCnt_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_AUTH_PLAYER);
		MakeMonitorQuery(&echoTemp.gamePlayerCnt_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_GAME_PLAYER);
		MakeMonitorQuery(&echoTemp.acceptTps_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_ACCEPT_TPS);
		MakeMonitorQuery(&echoTemp.recvTPS_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_PACKET_RECV_TPS);
		MakeMonitorQuery(&echoTemp.sendTPS_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_PACKET_SEND_TPS);
		MakeMonitorQuery(&echoTemp.authThreadFPS_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_AUTH_THREAD_FPS);
		MakeMonitorQuery(&echoTemp.gameThreadFPS_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_GAME_THREAD_FPS);
		MakeMonitorQuery(&echoTemp.packetPoolAlloced_, tableArr, (int)DBWriteType::GAME, dfMONITOR_DATA_TYPE_GAME_PACKET_POOL);
		++Cnt;
	}

	if (Cnt == 0)
		return;

	MakeMonitorQuery(&hardWareTemp.cpuTime_, tableArr, (int)DBWriteType::HARDWARE, dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL);
	MakeMonitorQuery(&hardWareTemp.nonPagedPoolAvaliableBytes_, tableArr, (int)DBWriteType::HARDWARE, dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY);
	MakeMonitorQuery(&hardWareTemp.netWorkRecvPerSec_, tableArr, (int)DBWriteType::HARDWARE, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV);
	MakeMonitorQuery(&hardWareTemp.netWorkSendPerSec_, tableArr, (int)DBWriteType::HARDWARE, dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND);
	MakeMonitorQuery(&hardWareTemp.memAvailable_, tableArr, (int)DBWriteType::HARDWARE, dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY);
}

void MonitorDbThread::OnWrite(Packet* pPacket)
{
	WORD jobType;
	(*pPacket) >> jobType;
	switch (en_MONITOR_DB_JOB(jobType))
	{
	case ONOFF:
		ProcessOnoff(pPacket);
		break;
	case MONITORWRITE:
	{
		ProcessMonitorWrite();
		break;
	}
	default:
		__debugbreak();
		break;
	}
}

void MonitorDbThread::ReqMonitorWrite()
{
	Packet* pPacket = MonitorDbThread::ALLOC();
	(*pPacket) << (WORD)en_MONITOR_DB_JOB::MONITORWRITE;
	ReqDBWriteJob(pPacket);
}

DBRequestTimer::DBRequestTimer(DWORD requestInterval, HANDLE hCompletionPort, LONG timeOutPqcsLimit, MonitorDbThread* pMonitorDbThread)
	:UpdateBase{ requestInterval,hCompletionPort,timeOutPqcsLimit }, pMonitorDbThread_{ pMonitorDbThread }
{
}

void DBRequestTimer::Update_IMPL()
{
	pMonitorDbThread_->ReqMonitorWrite();
}

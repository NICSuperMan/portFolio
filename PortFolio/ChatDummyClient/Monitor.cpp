#include <windows.h>
#include "Monitor.h"
#include <stdio.h>
#include <strsafe.h>
#include <unordered_map>

extern int g_maxSession;
extern int g_disconnectedNum;

extern int g_loginConnectTotal;
extern int g_loginDownClient;
extern int g_loginConnectFail;

extern int g_chatConnectTotal;
extern int g_chatDownClient;
extern int g_chatConnectFail;

extern int g_loopNum;

extern DWORD g_rttMin;
extern DWORD g_rttMax;
extern DWORD g_rttNum;
extern DWORD g_rttTotal;
extern float g_rttAvg;

char g_StartTimeArr[MAX_PATH];
char g_nowModeArr[MAX_PATH];

extern bool g_bPlay;
extern bool g_bConnectStop;
extern bool g_bDuplicateLoginOn;
extern bool g_bTestTimeOut;

static char* g_pEndOfNowMode;
static size_t g_RemainingLengthExceptNodeMode;

struct Player;
extern std::unordered_map<ULONGLONG, Player*> g_loginPlayerMap;
extern std::unordered_map<ULONGLONG, Player*> g_chatConnectPlayerMap;
extern std::unordered_map<ULONGLONG, Player*> g_chatLoginPlayerMap;

extern int g_replyWait;

extern int g_duplicateLoginTPS;
extern int g_sessionTimeOutTPS;
extern int g_loginTimeOutTPS;

extern DWORD g_loginTimeOutTotal;
extern DWORD g_loginTimeOutNum;

extern DWORD g_sessionTimeOutTotal;
extern DWORD g_sessionTimeOutNum;
extern int g_duplicateConnectFail;

void MonitorInit()
{
	SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);
	sprintf_s(g_StartTimeArr, MAX_PATH, "StartTime : %2d.%2d.%2d %2d:%2d:%2d", sysTime.wYear, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
	StringCchCopyExA(g_nowModeArr, MAX_PATH, "Now Mode : ", &g_pEndOfNowMode, &g_RemainingLengthExceptNodeMode, 0);
}

void MakeNowMode()
{
	char* pTemp = g_pEndOfNowMode;
	size_t len = g_RemainingLengthExceptNodeMode;

	if (g_bPlay)
		StringCchCopyExA(pTemp, len, "PLAY ", &pTemp, &len, 0);
	else
		StringCchCopyExA(pTemp, len, "STOP ", &pTemp, &len, 0);

	if(g_bConnectStop)
		StringCchCopyExA(pTemp, len, "| CONNECT STOP ", &pTemp, &len, 0);

	if(g_bDuplicateLoginOn)
		StringCchCopyExA(pTemp, len, "| DUPLICATE LOGIN ON ", &pTemp, &len, 0);

	if(g_bTestTimeOut)
		StringCchCopyExA(pTemp, len, "| TIMEOUT TEST ON", &pTemp, &len, 0);
}

void Monitor()
{
	MakeNowMode();
	size_t chatConnectNum = g_chatConnectPlayerMap.size();
	size_t   chatLoginNum = g_chatLoginPlayerMap.size();

	printf(
		"%s\n"
		"============================================================\n"
		"THREAD LOOP : %d\n"
		"%s\n\n"
		"------------------------------------------------------------\n"
		"S : STOP / PLAY  |  A : CONNECT STOP  |  W : DUPLICATE LOGIN ON / OFF | R : TIMEOUT TEST ON / OFF | Q : QUIT\n"
		"------------------------------------------------------------\n"
		"     Client Total : %d\n"
		"------------------------------------------------------------\n"
		"       Disconnect : %d\n"
		"------------------------------------------------------------\n"
		"  IN Login Server : %llu\n"
		"     ConnectTotal : %d\n"
		"      ConnectFail : %d\n"
		"------------------------------------------------------------\n"
		"   IN Chat Server : %llu\n"
		"     ConnectTotal : %d\n"
		"  Connect / Login : %llu / %llu\n"
		"      ConnectFail : %d\n"
		"  Chat DownClient : %d\n"
		"------------------------------------------------------------\n"
		"        ReplyWait : %d\n"
		"------------------------------------------------------------\n"
		"Action Delay Min : %6u ms\n"
		"Action Delay Max : %6u ms\n"
		"Action Delay Avr : %6.2f ms\n"
		"------------------------------------------------------------\n"
		"Duplicate Login Client Num : %6d, Duplicate Connect Fail : %d\n"
		"Session Timeout Client Num : %6d\n"
		"  Login Timeout Client Num : %6d\n"
		"------------------------------------------------------------\n"
		"Session Timeout Time Avg : %6.2f ms\n"
		"  Login Timeout Time Avg : %6.2f ms\n"
		"------------------------------------------------------------\n",
		g_StartTimeArr,
		g_loopNum,
		g_nowModeArr,
		g_maxSession,
		g_disconnectedNum,
		g_loginPlayerMap.size(),
		g_loginConnectTotal,
		g_loginConnectFail,
		chatConnectNum + chatLoginNum,
		g_chatConnectTotal,
		chatConnectNum, chatLoginNum,
		g_chatConnectFail,
		g_chatDownClient,
		g_replyWait,
		g_rttMin,
		g_rttMax,
		(g_rttNum == 0) ? 0 : (g_rttTotal / (float)g_rttNum),
		g_duplicateLoginTPS,g_duplicateConnectFail,
		g_sessionTimeOutTPS,
		g_loginTimeOutTPS,
		(g_sessionTimeOutNum == 0) ? 0 : (g_sessionTimeOutTotal / (float)g_sessionTimeOutNum),
		(g_loginTimeOutNum == 0) ? 0 : (g_loginTimeOutTotal / (float)g_loginTimeOutNum)
	);
}

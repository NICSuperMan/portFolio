#pragma once
#include <windows.h>

#ifdef LOGGERAPI
#else
#define LOGGERAPI extern "C" __declspec(dllimport)
#endif


enum LOG_LEVEL { DEBUG, SYSTEM, ERR };
#define CONSOLE 3
#define TEXTFILE 4 

LOGGERAPI void LOG(const WCHAR* pszType, LOG_LEVEL LogLevel, CHAR OUTPUT, CONST WCHAR* pszStringFormat, ...);
LOGGERAPI void LOG_MEMORY_VIEW(CONST WCHAR* pszType, LOG_LEVEL LogLevel, CHAR OUTPUT, BYTE* pBuffer, DWORD dwBufferLen, DWORD dwAlign, CONST WCHAR* pszStringFormat, ...);
LOGGERAPI void LOG_ASYNC(CONST WCHAR* pszStringFormat, ...);
LOGGERAPI void LOG_ASYNC_INIT();
LOGGERAPI void CLEAR_LOG_ASYNC();
LOGGERAPI void SET_LOG_LEVEL(LOG_LEVEL level);
LOGGERAPI LOG_LEVEL INCREASE_LOG_LEVEL();
LOGGERAPI LOG_LEVEL DECREASE_LOG_LEVEL();



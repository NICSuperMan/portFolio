#define LOGGERAPI extern "C" __declspec(dllexport)
#include"Logger.h"
#include <strsafe.h>
#include "LOG_BUF_INFO.h"
#include <process.h>



DWORD g_dwLogCount = 0;
LOG_LEVEL g_logLevel = (LOG_LEVEL)DEBUG;
#define LOCAL_LOG_BUF_SIZE 3000
#define ASYNC_LOG_ROW 1000000
#define ASYNC_LOG_COL ((300) * (sizeof(WCHAR)))
HANDLE g_hAsyncLogThreadEvent;
ULONGLONG g_ullAsyncIdx;
HANDLE g_hAsyncLogThread;

HANDLE g_hHeapHandle;
__declspec(thread) WCHAR* g_pszFolderPath;
__declspec(thread) LOG_BUF_INFO g_logBufInfo;

SRWLOCK g_srwForFILEIO;
SRWLOCK g_srwForLogLevel;

WCHAR (*g_AsyncLogArr)[ASYNC_LOG_COL / sizeof(WCHAR)];
DWORD g_dwStartIdx;

BOOL g_bFirstWritten = FALSE;

void GetParentDir(WCHAR* szTargetPath)
{
	WCHAR* lastSlash = wcsrchr(szTargetPath, L'\\');
	if (lastSlash)
		*lastSlash = L'\0';
}

void SYSLOG_DIRECTORY(CONST WCHAR* szPath)
{
	WCHAR exePath[MAX_PATH];
	WCHAR ParentDir[MAX_PATH];

	// 실행파일 이름 얻어오기
	GetModuleFileName(NULL, exePath, MAX_PATH);
	StringCchCopy(ParentDir, MAX_PATH, exePath);

	// 두단계 윗경로 얻어오기
	GetParentDir(ParentDir);
	GetParentDir(ParentDir);

	// 폴더이름 만들기
	StringCchPrintf(g_pszFolderPath, MAX_PATH, L"%s\\%s", ParentDir, szPath);

	// 폴더존재하면 넘어가고 없으면 만들기
	DWORD dwAttr = GetFileAttributes(g_pszFolderPath);
	if (dwAttr == INVALID_FILE_ATTRIBUTES)
	{
		if (!CreateDirectory(g_pszFolderPath, NULL))
		{
			DWORD dwErrCode = GetLastError();
			__debugbreak();
		}
	}
}

void LOG(CONST WCHAR* szHead, LOG_LEVEL LogLevel, CHAR OUTPUT, CONST WCHAR* szStringFormat, ...);


__forceinline void GetLogLevel(WCHAR* pOutLogLevelStr, LOG_LEVEL LogLevel)
{
	switch (LogLevel)
	{
	case DEBUG:
		memcpy(pOutLogLevelStr, L"DEBUG", sizeof(L"DEBUG"));
		break;
	case SYSTEM:
		memcpy(pOutLogLevelStr, L"SYSTEM", sizeof(L"SYSTEM"));
		break;
	case ERR:
		memcpy(pOutLogLevelStr, L"ERROR", sizeof(L"ERROR"));
		break;
	default:
		__debugbreak();
		break;
	}
}

__forceinline BOOL CheckLogLevel(LOG_LEVEL LogLevel)
{
	LOG_LEVEL level;
	AcquireSRWLockShared(&g_srwForLogLevel);
	level = g_logLevel;
	ReleaseSRWLockShared(&g_srwForLogLevel);
	if (level > LogLevel)
		return FALSE;
	else
		return TRUE;
}

// EX : [2024 - 08 - 15 01:30 : 50 /
__forceinline HRESULT MakeStringYearMonthDayHourMinuteSecond(SYSTEMTIME* pSt)
{
	GetLocalTime(pSt);

	// 연월일시분초 / 
	SIZE_T len = wcslen(g_logBufInfo.pLogBuf);
	HRESULT hResult = StringCchPrintf(g_logBufInfo.pLogBuf + len, (g_logBufInfo.iCurrentSize / 2) - len, L"[%04d-%02d-%02d %02d:%02d:%02d / ", pSt->wYear, pSt->wMonth, pSt->wDay, pSt->wHour, pSt->wMinute, pSt->wSecond);

	return hResult;
}

// EX : ERROR  / 000000001]  
// 인터락으로 로그카운트 올림
__forceinline HRESULT MakeLogLevelAndCount(LOG_LEVEL LogLevel)
{
	// 로그레벨 구하기
	SIZE_T len;
	WCHAR LogLevelStr[10];
	HRESULT hResult;
	GetLogLevel(LogLevelStr, LogLevel);

	len = wcslen(g_logBufInfo.pLogBuf);
	// 로그가 찍힌 순서를 멀티스레드 안전성잇게 파악하기 위해서 설정
	DWORD dwLogCount = InterlockedIncrement((LONG*)&g_dwLogCount);

	hResult = StringCchPrintf(g_logBufInfo.pLogBuf + len, (g_logBufInfo.iCurrentSize / 2) - len, L"%-6s / %09d]  ", LogLevelStr, dwLogCount);
	return hResult;
}

// 파일열고 지금까지 생성한 로그찍음.
// g_srwFILEIO를 가지고 이함수 호출 앞뒤에 락을 건다.
__forceinline void OpenFileAndLog(CONST WCHAR* pFilePath)
{
	FILE* pFile;
	_wfopen_s(&pFile, pFilePath, L"a");
	fputws(g_logBufInfo.pLogBuf, pFile);
	fputc(L'\n', pFile);
	fclose(pFile);
}

// 로그 찍다가 버퍼 부족할때 예외처리함수
// 버퍼 재할당후  재할당하기 이전 바이트수 -> 재할당한 바이트수를 로그로 찍고 끝.
// 호출한 함수에서는 이 함수호출한후 다시 맨처음부터 로그를 찍어야한다.
void ExceptLogBufInSuf(CONST WCHAR* pszType, LOG_LEVEL LogLevel, DWORD dwCurBufSize)
{
	while (1)
	{
		HeapFree(g_hHeapHandle, 0, g_logBufInfo.pLogBuf);
		g_logBufInfo.iCurrentSize *= 2;
		g_logBufInfo.pLogBuf = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, g_logBufInfo.iCurrentSize);
		StringCchPrintf(g_logBufInfo.pLogBuf, g_logBufInfo.iCurrentSize / 2, L"[%s]  ", pszType);
		SYSTEMTIME st;
		HRESULT hResult = MakeStringYearMonthDayHourMinuteSecond(&st);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType, ERR, g_logBufInfo.iCurrentSize);
			continue;
		}

		// 로그레벨 구하기
		// 로그가 찍힌 순서를 멀티스레드 안전성잇게 파악하기 위해서 설정
		MakeLogLevelAndCount(LogLevel);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType, ERR, g_logBufInfo.iCurrentSize);
			continue;
		}

		// 진짜 로깅하고자 하는것 가변인자로 넣기
		SIZE_T len = wcslen(g_logBufInfo.pLogBuf);
		hResult = StringCchPrintfW(g_logBufInfo.pLogBuf + len, (g_logBufInfo.iCurrentSize / 2) - len, L"LOG BUFSIZE INSUFFICIENT ALLOCATE % d -> % dBytes", dwCurBufSize, g_logBufInfo.iCurrentSize);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			continue;
		}

		// 파일 명 생성
		WCHAR FilePath[MAX_PATH];
		StringCchPrintf(FilePath, MAX_PATH, L"%s\\%04d%02d_%s.txt", g_pszFolderPath, st.wYear, st.wMonth, pszType);

		// 파일 열고 쓴다음 닫기
		// 여러개의 스레드들이 동시에 같은파일에 접근하면 파일열기에 실패할수도잇음
		AcquireSRWLockExclusive(&g_srwForFILEIO);
		OpenFileAndLog(FilePath);
		ReleaseSRWLockExclusive(&g_srwForFILEIO);
		break;
	}
}

LOGGERAPI void LOG(const WCHAR* pszType, LOG_LEVEL LogLevel, CHAR OUTPUT, CONST WCHAR* pszStringFormat, ...)
{
	if (!CheckLogLevel(LogLevel))
		return;

	while (1)
	{
		//[BATTLE]  같은 문자열 붙이기
		StringCchPrintf(g_logBufInfo.pLogBuf, g_logBufInfo.iCurrentSize / 2, L"[%s]  ", pszType);


		SYSTEMTIME st;
		HRESULT hResult = MakeStringYearMonthDayHourMinuteSecond(&st);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType,ERR,g_logBufInfo.iCurrentSize);
			continue;
		}

		// 로그레벨 구하기
		// 로그가 찍힌 순서를 멀티스레드 안전성잇게 파악하기 위해서 설정
		MakeLogLevelAndCount(LogLevel);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType,ERR,g_logBufInfo.iCurrentSize);
			continue;
		}

		// 진짜 로깅하고자 하는것 가변인자로 넣기
		SIZE_T len = wcslen(g_logBufInfo.pLogBuf);
		va_list va;
		va_start(va, pszStringFormat);
		hResult = StringCchVPrintf(g_logBufInfo.pLogBuf + len, (g_logBufInfo.iCurrentSize / 2) - len, pszStringFormat, va);
		va_end(va);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType,ERR,g_logBufInfo.iCurrentSize);
			continue;
		}
		if (OUTPUT == CONSOLE)
		{
			wprintf(L"%s\n", g_logBufInfo.pLogBuf);
		}
		else
		{
			// 파일 명 생성
			WCHAR FilePath[MAX_PATH];
			StringCchPrintf(FilePath, MAX_PATH, L"%s\\%04d%02d_%s.txt", g_pszFolderPath, st.wYear, st.wMonth, pszType);

			// 파일 열고 쓴다음 닫기
			// 여러개의 스레드들이 동시에 같은파일에 접근하면 파일열기에 실패할수도잇음
			AcquireSRWLockExclusive(&g_srwForFILEIO);
			OpenFileAndLog(FilePath);
			ReleaseSRWLockExclusive(&g_srwForFILEIO);
		}
		break;
	}
}

#define ADDRESS 16
#define BYTESTRING 2
#define SPACEBAR 1
#define NULL_TERMINATOR 1
#define ENTER 1
#define LinePlaceHolderInterval (dwAlign + 1)


//swprintf_s는 2번째 매개변수 BufferCount에 NULL Terminator를 쓸 곳까지 포함해서 넣어야한다.
//dwNumOfChar는 NULL Terminator까지 포함해서 계산한 버퍼사이즈이기 때문에 dwNumOfChar - 1 - dwWrittenWcharNum이 아닌 1큰값을 사용함.
void MAKE_BINARY_LOG(BYTE* pBuffer, DWORD dwBufferLen, DWORD dwAlign, CONST WCHAR* pFilePath)
{
	DWORD dwLineNumber;
	DWORD dwLastLineByteNum;
	DWORD dwNumOfWChar;
	WCHAR* pTempFileBuf;
	DWORD dwBinaryBufCnt;
	DWORD dwWrittenWcharNum;
	DWORD dwLineCounter;

	dwLineNumber = dwBufferLen / dwAlign; // 실제 줄수 보다 1개작다, 마지막 라인이 제외됨.
	dwLastLineByteNum = dwBufferLen % dwAlign;


	// 줄수 * (주소 + (스페이스바 ~ 바이트값 * 줄당 바이트값 갯수) * 줄수 + 널문자)
	dwNumOfWChar = dwLineNumber * (ADDRESS + ((BYTESTRING + SPACEBAR) * dwAlign + ENTER)) +
		(dwLastLineByteNum > 0 ? 1 : 0) * (ADDRESS + dwLastLineByteNum * (BYTESTRING + SPACEBAR)) + NULL_TERMINATOR; // 마지막줄
	pTempFileBuf = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, dwNumOfWChar * sizeof(WCHAR));

	dwBinaryBufCnt = 0;
	dwWrittenWcharNum = 0;
	dwLineCounter = 0;

	while (dwLineCounter < dwLineNumber)
	{
		swprintf_s(pTempFileBuf + dwWrittenWcharNum, dwNumOfWChar - dwWrittenWcharNum, L"%p", pBuffer + dwBinaryBufCnt);
		dwWrittenWcharNum += ADDRESS;
		for (DWORD i = 0; i < dwAlign; ++i)
		{
			swprintf_s(pTempFileBuf + dwWrittenWcharNum, dwNumOfWChar  - dwWrittenWcharNum, L" %02x", pBuffer[dwBinaryBufCnt]);
			dwWrittenWcharNum += (BYTESTRING + SPACEBAR);
			++dwBinaryBufCnt;
		}
		pTempFileBuf[dwWrittenWcharNum] = L'\n';
		++dwWrittenWcharNum;
		++dwLineCounter;
		pTempFileBuf[dwWrittenWcharNum] = L'\0';
	}

	if (dwLastLineByteNum == 0)
		goto lb_logging;

	// 마지막줄 처리
	swprintf_s(pTempFileBuf + dwWrittenWcharNum, dwNumOfWChar  - dwWrittenWcharNum, L"%p", pBuffer + dwBinaryBufCnt);
	dwWrittenWcharNum += ADDRESS;
	for (DWORD i = 0; dwBinaryBufCnt < dwBufferLen; ++i)
	{
		swprintf_s(pTempFileBuf + dwWrittenWcharNum, dwNumOfWChar - dwWrittenWcharNum, L" %02x", pBuffer[dwBinaryBufCnt]);
		dwWrittenWcharNum += (BYTESTRING + SPACEBAR);
		++dwBinaryBufCnt;
	}

	lb_logging:
	FILE* pFile;
	_wfopen_s(&pFile, pFilePath, L"a");
	fputws(pTempFileBuf, pFile);
	fputc(L'\n', pFile);
	fclose(pFile);

	HeapFree(g_hHeapHandle, 0, pTempFileBuf);
}

LOGGERAPI void LOG_MEMORY_VIEW(CONST WCHAR* pszType, LOG_LEVEL LogLevel, CHAR OUTPUT, BYTE* pBuffer, DWORD dwBufferLen, DWORD dwAlign, CONST WCHAR* pszStringFormat, ...)
{
	if (!CheckLogLevel(LogLevel))
		return;

	while (1)
	{
		//[BATTLE]  같은 문자열 붙이기
		StringCchPrintf(g_logBufInfo.pLogBuf, g_logBufInfo.iCurrentSize / 2, L"[%s]  ", pszType);


		SYSTEMTIME st;
		HRESULT hResult = MakeStringYearMonthDayHourMinuteSecond(&st);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType, ERR, g_logBufInfo.iCurrentSize);
			continue;
		}

		// 로그레벨 구하기
		// 로그가 찍힌 순서를 멀티스레드 안전성잇게 파악하기 위해서 설정
		MakeLogLevelAndCount(LogLevel);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType, ERR, g_logBufInfo.iCurrentSize);
			continue;
		}

		// 진짜 로깅하고자 하는것 가변인자로 넣기
		SIZE_T len = wcslen(g_logBufInfo.pLogBuf);
		va_list va;
		va_start(va, pszStringFormat);
		hResult = StringCchVPrintf(g_logBufInfo.pLogBuf + len, (g_logBufInfo.iCurrentSize / 2) - len, pszStringFormat, va);
		va_end(va);
		if (hResult == STRSAFE_E_INSUFFICIENT_BUFFER)
		{
			ExceptLogBufInSuf(pszType, ERR, g_logBufInfo.iCurrentSize);
			continue;
		}


		if (OUTPUT == CONSOLE)
		{
			wprintf(L"%s\n", g_logBufInfo.pLogBuf);
		}
		else
		{
			// 파일 명 생성
			WCHAR FilePath[MAX_PATH];
			StringCchPrintf(FilePath, MAX_PATH, L"%s\\%04d%02d_%s.txt", g_pszFolderPath, st.wYear, st.wMonth, pszType);

			// 파일 열고 쓴다음 닫기
			// 여러개의 스레드들이 동시에 같은파일에 접근하면 파일열기에 실패할수도잇음
			AcquireSRWLockExclusive(&g_srwForFILEIO);
			OpenFileAndLog(FilePath);
			MAKE_BINARY_LOG(pBuffer, dwBufferLen, dwAlign, FilePath);
			ReleaseSRWLockExclusive(&g_srwForFILEIO);
		}
		break;
	}
}

WCHAR TempArr[100 * (ASYNC_LOG_COL / 2)];

unsigned __stdcall AsyncLogThreadProc(void* pParam)
{
	while (1)
	{
		DWORD dwState = WaitForSingleObject(g_hAsyncLogThreadEvent, 1000);
		if (dwState == WAIT_OBJECT_0)
			break;

		if (InterlockedExchange((LONG*)&g_bFirstWritten, g_bFirstWritten) == FALSE)
			continue;

		ULONGLONG ullTemp = _InterlockedExchange(&g_ullAsyncIdx, g_ullAsyncIdx);
		// dwStartIdx 부터 dwLastIdx 까지 딱 쓸거임 즉 dwStartIdx == dwLastIdx일때까지 쓰고 dwLastIdx에서 한번 더 쓴후 g_dwStartIdx = dwStartIdx 저장하면 끝
		// 따라서 깨어낫을땐 g_dwStartIdx를 바로 저장하고 거기서부터 쓰기 시작하면 된다.
		DWORD dwStartIdx = g_dwStartIdx;

		// dwLastIdx 까지 현재 써져잇는것이다.
		DWORD dwLastIdx = (ullTemp - 1) % ASYNC_LOG_ROW;
		SYSTEMTIME st;
		GetLocalTime(&st);

		WCHAR FilePath[MAX_PATH];
		StringCchPrintf(FilePath, MAX_PATH, L"%s\\%04d%02d_ASYNC.txt", g_pszFolderPath, st.wYear, st.wMonth);

		FILE* pFile;
		_wfopen_s(&pFile, FilePath, L"a");

		WCHAR* logArr;
		while (dwLastIdx != dwStartIdx)
		{
			logArr = g_AsyncLogArr[dwStartIdx];
			// 혹시모르니 마지막자리에 nullTerminator String 넣기
			logArr[(ASYNC_LOG_COL / sizeof(WCHAR)) - 1] = L'\0';
			fputws(logArr, pFile);
			dwStartIdx = (dwStartIdx + 1) % ASYNC_LOG_ROW;
		}
		// 마지막
		logArr = g_AsyncLogArr[dwStartIdx];
		logArr[(ASYNC_LOG_COL / 2) - 1] = L'\0';
		fputws(logArr, pFile);
		g_dwStartIdx = (dwLastIdx + 1) % ASYNC_LOG_ROW;
		fclose(pFile);
		InterlockedExchange((LONG*)&g_bFirstWritten, FALSE);
	}
	return 0;
}
LOGGERAPI void LOG_ASYNC_INIT()
{
	g_AsyncLogArr = (WCHAR(*)[ASYNC_LOG_COL / sizeof(WCHAR)])HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, ASYNC_LOG_ROW * ASYNC_LOG_COL * 2);
	g_hAsyncLogThreadEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, AsyncLogThreadProc, nullptr, 0, nullptr);
}

LOGGERAPI void CLEAR_LOG_ASYNC()
{
	HeapFree(g_hHeapHandle, 0, g_AsyncLogArr);
	SetEvent(g_hAsyncLogThreadEvent);
	WaitForSingleObject(g_hAsyncLogThread, INFINITE);
}

LOGGERAPI void LOG_ASYNC(CONST WCHAR* pszStringFormat, ...)
{
	DWORD dwIdx;
	WCHAR* logArr;
	HRESULT hResult;
	va_list va;
	SIZE_T len;

	ULONGLONG ullAsyncLogCount = InterlockedIncrement((LONG*)&g_ullAsyncIdx);


	// 현재 쓸 인덱스
	dwIdx = (ullAsyncLogCount - 1) % ASYNC_LOG_ROW;
	logArr = g_AsyncLogArr[dwIdx];

	hResult = StringCchPrintf(logArr, ASYNC_LOG_COL / 2, L"[%09llu] Thread ID : %u ", ullAsyncLogCount, GetCurrentThreadId());
	len = wcslen(logArr);

	va_start(va, pszStringFormat);
	hResult = StringCchVPrintf(logArr + len, (ASYNC_LOG_COL / 2) - len, pszStringFormat, va);
	va_end(va);
	len = wcslen(logArr);
	logArr[len] = L'\n';
	logArr[len + 1] = L'\0';
	InterlockedExchange((LONG*)&g_bFirstWritten, TRUE);
}

LOGGERAPI void SET_LOG_LEVEL(LOG_LEVEL level)
{
	AcquireSRWLockExclusive(&g_srwForLogLevel);
	if (level >= DEBUG && level <= ERR)
		g_logLevel = level;
	ReleaseSRWLockExclusive(&g_srwForLogLevel);
}

LOGGERAPI LOG_LEVEL INCREASE_LOG_LEVEL()
{
	LOG_LEVEL Ret;
	AcquireSRWLockExclusive(&g_srwForLogLevel);
	if (g_logLevel >= DEBUG && g_logLevel <= SYSTEM)
	{
		++*(DWORD*)(&g_logLevel);
		Ret = g_logLevel;
	}
	else
	{
		Ret = g_logLevel;
	}
	ReleaseSRWLockExclusive(&g_srwForLogLevel);
	return Ret;
}

LOGGERAPI LOG_LEVEL DECREASE_LOG_LEVEL()
{
	LOG_LEVEL Ret;
	AcquireSRWLockExclusive(&g_srwForLogLevel);
	if (g_logLevel >= SYSTEM && g_logLevel <= ERR)
	{
		--*(DWORD*)(&g_logLevel);
		Ret = g_logLevel;
	}
	else
	{
		Ret = g_logLevel;
	}
	ReleaseSRWLockExclusive(&g_srwForLogLevel);
	return Ret;
}



// dllmain.cpp : DLL 애플리케이션의 진입점을 정의합니다.
#include "pch.h"
#include <windows.h>
#include "LOG_BUF_INFO.h"

#define LOCAL_LOG_BUF_SIZE 3000

extern __declspec(thread) LOG_BUF_INFO g_logBufInfo;
extern __declspec(thread) WCHAR* g_pszFolderPath;
extern HANDLE g_hHeapHandle;
extern SRWLOCK g_srwForFILEIO;
extern SRWLOCK g_srwForLogLevel;

void SYSLOG_DIRECTORY(const WCHAR* szPath);



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		InitializeSRWLock(&g_srwForFILEIO);
		InitializeSRWLock(&g_srwForLogLevel);
        g_hHeapHandle = HeapCreate(0, 0, 0);
        if (!g_hHeapHandle)
            return FALSE;
        
        // 로그용 버퍼 초기길이 설정 및 할당, 폴더 경로 할당
        g_logBufInfo.iCurrentSize = LOCAL_LOG_BUF_SIZE * sizeof(WCHAR);
        g_logBufInfo.pLogBuf = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, g_logBufInfo.iCurrentSize);
        g_pszFolderPath = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, MAX_PATH * sizeof(WCHAR));
        SYSLOG_DIRECTORY(L"LOG");
        break;
    case DLL_THREAD_ATTACH:
        // 로그용 버퍼 초기길이 설정 및 할당, 폴더 경로 할당
		g_logBufInfo.iCurrentSize = LOCAL_LOG_BUF_SIZE * sizeof(WCHAR);
        g_logBufInfo.pLogBuf = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, g_logBufInfo.iCurrentSize);
        g_pszFolderPath = (WCHAR*)HeapAlloc(g_hHeapHandle, HEAP_GENERATE_EXCEPTIONS, MAX_PATH * sizeof(WCHAR));
        SYSLOG_DIRECTORY(L"LOG");
        break;
    case DLL_THREAD_DETACH:
        // 로그용 버퍼 할당해제, 폴더경로 할당해제
        HeapFree(g_hHeapHandle, 0, g_logBufInfo.pLogBuf);
        HeapFree(g_hHeapHandle, 0, g_pszFolderPath);
        break;
    case DLL_PROCESS_DETACH:
        // 로그용 버퍼 할당해제, 폴더경로 할당해제, 로그DLL 전용힙 파괴
        HeapFree(g_hHeapHandle, 0, g_logBufInfo.pLogBuf);
        HeapFree(g_hHeapHandle, 0, g_pszFolderPath);
        HeapDestroy(g_hHeapHandle);
        break;
    }
    return TRUE;
}


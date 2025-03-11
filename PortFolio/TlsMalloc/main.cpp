#include <windows.h>
#include "CTlsObjectPool.h"
#include <stdio.h>
#include <process.h>
#include "MultithreadProfiler.h"
#pragma comment(lib,"Winmm.lib")

constexpr int COUNT = 1000;
constexpr int ITERATION = 1000;

HANDLE g_startEvent;

constexpr int DATA_SIZE = 512 + 128 * 0;
struct TEST
{
	char temp[DATA_SIZE];
	TEST()
	{
	}
};

HANDLE g_threadArr[20];
HANDLE g_hEventArr[20];

#define MALLOC

#ifdef MALLOC
#define ALLOC() (TEST*)malloc(sizeof(TEST))
#define RELEASE(addr) free(addr)
#else
CTlsObjectPool<TEST, false> g_pool;
#define ALLOC() g_pool.Alloc()
#define RELEASE(addr) g_pool.Free(addr);
#endif


unsigned __stdcall threadProc(void* pParam)
{
	TEST* tempArr[COUNT];
	int idx = (int)pParam;
	SetEvent(g_hEventArr[idx]);
	WaitForSingleObject(g_startEvent, INFINITE);

	for (int i = 0; i < ITERATION; ++i)
	{
		{
			PROFILE(1, "Alloc");
			for (int i = 0; i < COUNT; ++i)
			{
				tempArr[i] = ALLOC();
			}
		}

		{
			PROFILE(1, "Release");
			for (int i = 0; i < COUNT; ++i)
			{
				RELEASE(tempArr[i]);
			}
		}
	}
	return 0;
}


int main()
{
	timeBeginPeriod(1);
	int threadNum;
	scanf_s("%d", &threadNum);
	g_startEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_startEvent)
	{
		DWORD errCode = GetLastError();
		__debugbreak();
	}

	for (int i = 0; i < threadNum; ++i)
	{
		g_hEventArr[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!g_hEventArr[i])
		{
			DWORD errCode = GetLastError();
			__debugbreak();
		}
	}

	char fileName[MAX_PATH];
#ifdef MALLOC
	sprintf_s(fileName, MAX_PATH, "Data - %d, MALLOC ThreadNUM - %d, Bucket Size - %llu, len - %d.txt", DATA_SIZE, threadNum, sizeof(Bucket<TEST, false>), Bucket<TEST, false>::size);
#else
	sprintf_s(fileName, MAX_PATH, "Data - %d, TLSPOOL ThreadNUM - %d, Bucket Size - %llu, len - %d.txt", DATA_SIZE, threadNum, sizeof(Bucket<TEST, false>), Bucket<TEST, false>::size);
	TEST** pArr = (TEST**)malloc(sizeof(TEST*) * COUNT * ITERATION * threadNum);
	for (int i = 0; i < COUNT * threadNum; ++i)
	{
		pArr[i] = g_pool.Alloc();
	}

	for (int i = 0; i < COUNT * threadNum; ++i)
	{
		g_pool.Free(pArr[i]);
	}
	free(pArr);
#endif
	printf("%s", fileName);

	for (int i = 0; i < threadNum; ++i)
	{
		g_threadArr[i] = (HANDLE)_beginthreadex(nullptr, 0, threadProc, (void*)i, 0, nullptr);
	}

	// 모든스레드가 생성되어 진입점함수를 시작할떄까지 대기
	WaitForMultipleObjects(threadNum, g_hEventArr, TRUE, INFINITE);
	SetEvent(g_startEvent);

	WaitForMultipleObjects(threadNum, g_threadArr, TRUE, INFINITE);
	PROFILER::ProfileDataOutText(fileName);
	return 0;
}
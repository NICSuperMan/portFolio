#include "CLockFreeQueue.h"

#include "MultithreadProfiler.h"
#include "iostream"
#include "SpinQ.h"
#include "CSQ.h"
#include "CMutexQueue.h"
#include "process.h"
#pragma comment(lib,"Winmm.lib")

using namespace std;
SpinQ<int> g_spinQ;
CLockFreeQueue<int> g_LFQ;
CsQ<int> g_csQ;
CMutexQueue<int> g_cmQ;

constexpr int COUNT = 4 * 100;
int THREAD_NUM;


HANDLE g_startEvent;
HANDLE g_thrHandleArr[20];
HANDLE g_hEventArr[20];
constexpr int ITERATION = 10;
//#define MUTEX

#ifdef SPIN
#define ENQ(num) g_spinQ.enqueue(num)
#define DEQ() g_spinQ.dequeue()
#elif defined LOCKFREE
#define ENQ(num) g_LFQ.Enqueue(num)
#define DEQ() g_LFQ.Dequeue()
#elif defined MUTEX
#define ENQ(num) g_cmQ.enqueue(num)
#define DEQ() g_cmQ.dequeue()
#else
#define ENQ(num) g_csQ.enqueue(num)
#define DEQ() g_csQ.dequeue()
#endif

LONG g_i = 0;

unsigned newTest(void* pParam)
{
	int idx = (int)pParam;

	// 메인스레드를 깨우기 위함 
	SetEvent(g_hEventArr[idx]);

	// 모든스레드가 거의 동시에 시작하게 하기위해서 수동리셋이벤트를 대기함(메인이 SetEvent)
	WaitForSingleObject(g_startEvent, INFINITE);
	while (true)
	{
		for (int i = 0; i < ITERATION * COUNT; ++i)
		{
			PROFILE(1, "ENQ")
			ENQ(i);
		}

		for (int i = 0; i < ITERATION * COUNT; ++i)
		{
			PROFILE(1, "DEQ")
			DEQ();
		}
	}
}

int main()
{
	timeBeginPeriod(1);
	scanf_s("%d", &THREAD_NUM);
	g_startEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // 수동리셋
	if (!g_startEvent)
	{
		DWORD errCode = GetLastError();
		__debugbreak();
	}

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_hEventArr[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	printf("THREAD_NUM : %d Start\n", THREAD_NUM);
	PROFILER::Init();

	// 테스트 도중 동적할당을 제거하기 위해 
	for (int i = 0; i < ITERATION * COUNT * THREAD_NUM; ++i)
		ENQ(i);

	for (int i = 0; i < ITERATION * COUNT * THREAD_NUM; ++i)
		DEQ();

	for (int i = 0; i < THREAD_NUM; ++i)
	{
		g_thrHandleArr[i] = (HANDLE)_beginthreadex(nullptr, 0, newTest, (void*)i, 0, nullptr);
		if (!g_thrHandleArr[i])
		{
			DWORD errCode = GetLastError();
			__debugbreak();
		}
	}


	// 모든 스레드들이 최대한 같이 시작하기 위해서 SetEvent하는 이벤트를 대기
	cout << "Press Enter To Clear Data!" << endl;
	cout << "Press 0 To Data TextOut!" << endl;

	// 모든스레드가 생성되어 진입점함수를 시작할떄까지 대기
	WaitForMultipleObjects(THREAD_NUM, g_hEventArr, TRUE, INFINITE);
	// 수동 리셋 이벤트를 셋해서 모두를 깨운다
	SetEvent(g_startEvent);

	// 종료를 대기 -> 일정주기로 타임아웃되어 키입력을받아 성능프로파일링 결과를 초기화하거나 출력
	while (WaitForMultipleObjects(THREAD_NUM, g_thrHandleArr, TRUE, 1000) == WAIT_TIMEOUT)
	{
		if (GetAsyncKeyState(VK_RETURN) & 0x01)
		{
			cout << "Profile Data Clear" << endl;
			PROFILER::Reset();
		}
		else if (GetAsyncKeyState(0x30) & 0x01)
		{
			cout << "Profile Data TextOut" << endl;
#ifdef SPIN
			PROFILER::ProfileDataOutText("spinQ.txt");
#elif defined LOCKFREE
			PROFILER::ProfileDataOutText("LFQ.txt");
#elif defined MUTEX
			PROFILER::ProfileDataOutText("Mutex.txt");
#else
			PROFILER::ProfileDataOutText("CS.txt");
#endif
		}
	}

	CloseHandle(g_startEvent);
	for (int i = 0; i < THREAD_NUM; ++i)
		CloseHandle(g_thrHandleArr[i]);
	return 0;
}
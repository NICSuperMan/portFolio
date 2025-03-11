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

	// ���ν����带 ����� ���� 
	SetEvent(g_hEventArr[idx]);

	// ��罺���尡 ���� ���ÿ� �����ϰ� �ϱ����ؼ� ���������̺�Ʈ�� �����(������ SetEvent)
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
	g_startEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // ��������
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

	// �׽�Ʈ ���� �����Ҵ��� �����ϱ� ���� 
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


	// ��� ��������� �ִ��� ���� �����ϱ� ���ؼ� SetEvent�ϴ� �̺�Ʈ�� ���
	cout << "Press Enter To Clear Data!" << endl;
	cout << "Press 0 To Data TextOut!" << endl;

	// ��罺���尡 �����Ǿ� �������Լ��� �����ҋ����� ���
	WaitForMultipleObjects(THREAD_NUM, g_hEventArr, TRUE, INFINITE);
	// ���� ���� �̺�Ʈ�� ���ؼ� ��θ� �����
	SetEvent(g_startEvent);

	// ���Ḧ ��� -> �����ֱ�� Ÿ�Ӿƿ��Ǿ� Ű�Է����޾� �����������ϸ� ����� �ʱ�ȭ�ϰų� ���
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
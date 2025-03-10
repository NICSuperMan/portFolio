#include <WinSock2.h>
#include <windows.h>
#include "Scheduler.h"
#include "Logger.h"
#include "Assert.h"
#include "process.h"

#include "UpdateBase.h"
#pragma comment(lib,"LoggerMt.lib")

namespace Scheduler
{
	const MYOVERLAPPED TimerPostOverlapped{ OVERLAPPED{},OVERLAPPED_REASON::UPDATE };
	HANDLE hThread;
	UpdatePQCSInfo uPI;
}

unsigned __stdcall Scheduler::threadFunc(void* pParam)
{
	int curNum = uPI.currentNum_;
	UpdateBase** ppUpdateArr = uPI.pUpdateArr_;
	int arr[UpdatePQCSInfo::len];

	uPI.firstTimeInit(timeGetTime());

	// 첫 PQCS
	for (int i = 0; i < curNum; ++i)
	{
		UpdateBase* pBase = ppUpdateArr[i];
		InterlockedIncrement(&pBase->unProcessedPqcsCnt_);
		PostQueuedCompletionStatus(pBase->hcp_, 2, (ULONG_PTR)pBase, (LPOVERLAPPED)&TimerPostOverlapped);
	}

	int min;
	{
		// 가장 먼저 깨워야할 사람찾기
		DWORD tempTimeStamp = timeGetTime();
		for (int i = 0; i < curNum; ++i)
		{
			UpdateBase* pBase = ppUpdateArr[i];
			arr[i] = pBase->scdr.UpdateAndGetTimeToSleep(tempTimeStamp, pBase->TICK_PER_FRAME_);
		}

		min = *std::min_element(arr, arr + curNum);
		if (WaitForSingleObject(uPI.hTerminateEvent_, min) != WAIT_TIMEOUT)
			return 0;
	}

	while (1)
	{
		DWORD tempTime = timeGetTime();
		for (int i = 0; i < curNum; ++i)
		{
			arr[i] -= min;
			UpdateBase* pBase = ppUpdateArr[i];
			if (arr[i] <= 0) // 배열에 기록된 대기시간에서 이전에 구한 대기시간의 최소값을 빼서 0이하가 되는 요소는 전부 스케줄링 대상임
			{
				if (pBase->unProcessedPqcsCnt_ < pBase->AllowedUnProcessedPqcsLimit) // IOCP 큐가 빨리 빨리 비워지지않아서 처리되지않은 완료패킷의 수가 일정수를 넘어가면 PQCS를 쏘지 않는다
				{
					InterlockedIncrement(&pBase->unProcessedPqcsCnt_);
					PostQueuedCompletionStatus(pBase->hcp_, 2, (ULONG_PTR)pBase, (LPOVERLAPPED)&Scheduler::TimerPostOverlapped);
				}
				arr[i] = pBase->scdr.UpdateAndGetTimeToSleep(tempTime, pBase->TICK_PER_FRAME_); // 다음 PQCS까지 남은시간을 계산해서 배열에 대입
			}
		}
		min = *std::min_element(arr, arr + curNum); // 가장 PQCS의 대기시간중 가장 짧은시간을 구해서 그떄까지 잠든다
		if (WaitForSingleObject(uPI.hTerminateEvent_, min) != WAIT_TIMEOUT)
			break;
	}
    return 0;
}

void Scheduler::Register_UPDATE(UpdateBase* pUpdate)
{
	uPI.pUpdateArr_[uPI.currentNum_++] = pUpdate;
	if (uPI.currentNum_ >= UpdatePQCSInfo::len) __debugbreak();
}


void Scheduler::Init()
{
	hThread = (HANDLE)_beginthreadex(nullptr, 0, threadFunc, nullptr, CREATE_SUSPENDED, nullptr);
}

void Scheduler::Release_SchedulerThread()
{
	// Lan,Net 서버에서의 ShutDown에서 각각호출시 한번만 처리되도록 예외처리
	// INVALID_HANDLE_VALUE에 대해 WaitFor을 하면 무한대기하는 현상을 발견
	if (hThread == INVALID_HANDLE_VALUE)
		return;
	SetEvent(uPI.hTerminateEvent_);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	hThread = INVALID_HANDLE_VALUE;
}


void Scheduler::Start()
{
	ResumeThread(hThread);
}

const MYOVERLAPPED* Scheduler::GetUpdateOverlapped()
{
	return &TimerPostOverlapped;
}


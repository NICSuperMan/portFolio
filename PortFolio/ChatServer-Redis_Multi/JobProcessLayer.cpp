#include <WinSock2.h>
#include "LoginChatServer.h"
#include "JobProcessLayer.h"
#include "Job.h"
#include "CLockFreeQueue.h"

extern CLockFreeQueue<JOB*> g_jobQ;

JobProcessLayer::JobProcessLayer(DWORD tickPerFrame, HANDLE hCompletionPort, LONG pqcsLimit, LoginChatServer* pChatServer)
	:UpdateBase{ tickPerFrame,hCompletionPort,pqcsLimit }, pChatServer_{ pChatServer }
{
}

JobProcessLayer::~JobProcessLayer()
{
}

void JobProcessLayer::Update_IMPL()
{
	LONG compare = 0;
	while (1)
	{
		const auto& opt = g_jobQ.Dequeue();
		if (!opt.has_value())
			break;

		JOB* pJob = opt.value();
		// �˸��� ��и� jobQ�� job�й� -> ���������� jobó�������尡 Job�� Packet ������
		pJob->distributeJobToOrderQ();
	}

	InterlockedExchange(&pChatServer_->updateThreadWakeCount_, 4);
	PostQueuedCompletionStatus(hcp_, 1, 0, (LPOVERLAPPED)&pChatServer_->OnPostOverlapped);
	PostQueuedCompletionStatus(hcp_, 1, 1, (LPOVERLAPPED)&pChatServer_->OnPostOverlapped);
	PostQueuedCompletionStatus(hcp_, 1, 2, (LPOVERLAPPED)&pChatServer_->OnPostOverlapped);
	PostQueuedCompletionStatus(hcp_, 1, 3, (LPOVERLAPPED)&pChatServer_->OnPostOverlapped);

	DWORD ret = WaitForSingleObject(pChatServer_->hUpdateThreadWakeEvent_, INFINITE);

	if (WAIT_OBJECT_0 != ret)
	{
		auto err = GetLastError();
		__debugbreak();
	}
}

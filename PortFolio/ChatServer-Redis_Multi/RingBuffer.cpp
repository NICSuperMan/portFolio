#include "RingBuffer.h"
#include <memory.h>

#pragma warning(disable : 4700)

// 특이사항 : 원형 큐이기 때문에 할당 크기가 BUFFER_SIZE 보다 1 커야함
#pragma warning(disable : 26495)
RingBuffer::RingBuffer(void)
{
	iOutPos_ = iInPos_ = 0;
}
#pragma warning(default : 26495)

RingBuffer::~RingBuffer(void)
{
}

// Return:  (int) 현재 Buffer에서 Enqueue되어있는 크기
int RingBuffer::GetUseSize(void)
{
	int iRet;
	int in = iInPos_;
	int out = iOutPos_;
	GetUseSize_MACRO(in, out, iRet);
	return iRet;
}

// Return:(int)현재 Buffer에 추가로 Enequeue 가능한 크기
int RingBuffer::GetFreeSize(void)
{
	int iRet;
	int in = iInPos_;
	int out = iOutPos_;
	GetFreeSize_MACRO(in, out, iRet);
	return iRet;
}

//--------------------------------------------------------------------
// 기능: Ring Buffer에 Data 삽입
// Parameters:	IN (char *) Ring Buffer에 Data를 넣을 대상 Buffer
//				IN (int) 복사할 크기
// Return:		(int) Ring Buffer에 들어간 크기(사실상 0 혹은 sizeToPut 둘중 하나다)
// 특이사항 : Enqueue정책은 sizeToPut() > GetFreeSize()이면 0을 반환한다.
//--------------------------------------------------------------------
int RingBuffer::Enqueue(const char* pSource, int iSizeToPut)
{
	int iFreeSize;
	int iDirectEnqueueSize;
	int iFirstSize;
	int iSecondSize;
	char* pInStartPos;

	int in = iInPos_;
	int out = iOutPos_;

	GetFreeSize_MACRO(in, out, iFreeSize);
	if (iSizeToPut > iFreeSize)
	{
		// 반환하는 쪽에서는 연결을 끊어버려야함.
		return 0;
	}
	GetDirectEnqueueSize_MACRO(in, out, iDirectEnqueueSize);

	// 직선으로 인큐 가능한사이즈가 넣으려는 사이즈보다 크거나 같으면 한번만 복사
	if (iDirectEnqueueSize >= iSizeToPut) 
		iFirstSize = iSizeToPut;
	else // 두번에 나눠서 복사해야함.
		iFirstSize = iDirectEnqueueSize;

	GetInStartPtr_MACRO(in, pInStartPos);
	memcpy(pInStartPos, pSource, iFirstSize);
	MoveInOrOutPos_MACRO(in, iFirstSize);

	iSecondSize = iSizeToPut - iFirstSize;
	if (iSecondSize <= 0)
	{
		iInPos_ = in;
		return iFirstSize;
	}

	GetInStartPtr_MACRO(in, pInStartPos);
	memcpy(pInStartPos, pSource + iFirstSize, iSecondSize);
	MoveInOrOutPos_MACRO(in, iSecondSize);
	iInPos_ = in;

	return iFirstSize + iSecondSize;
}

//--------------------------------------------------------------------
// 기능: Ring Buffer에서 Data 꺼냄
// Parameters:	OUT (char *) Ring Buffer의 Data를 받을 대상 Buffer
//				IN (int) 복사할 크기
// Return:		(int) Ring Buffer에서 꺼내서 pDest에 복사한 데이터의 크기
// 특이사항 : Dequeue정책은 sizeToRead > GetUseSize()이면 Dequeue를 제대로 수행하지않고 0을 반환한다.
//--------------------------------------------------------------------
int RingBuffer::Dequeue(char* pOutDest, int iSizeToOut)
{
	int iUseSize;
	int iDirectDequeueSize;
	int iFirstSize;
	int iSecondSize;
	char* pOutStartPos;

	int in = iInPos_;
	int out = iOutPos_;

	GetUseSize_MACRO(in, out, iUseSize);
	if (iSizeToOut > iUseSize)
	{
		// 들어있는 데이터보다 많은 데이터를 읽으려고 하면 그냥 반환한다.
		return 0;
	}
	GetDirectDequeueSize_MACRO(in, out, iDirectDequeueSize);

	// 직선으로 디큐 가능한사이즈가 읽으려는 사이즈보다 크거나 같으면 한번만 복사
	if (iDirectDequeueSize > iSizeToOut) 
		iFirstSize = iSizeToOut;
	else // 두번에 나눠서 복사해야함
		iFirstSize = iDirectDequeueSize;


	GetOutStartPtr_MACRO(out, pOutStartPos);
	memcpy(pOutDest, pOutStartPos, iFirstSize);
	MoveInOrOutPos_MACRO(out, iFirstSize);

	iSecondSize = iSizeToOut - iFirstSize;
	if (iSecondSize <= 0)
	{
		iOutPos_ = out;
		return iFirstSize;
	}

	GetOutStartPtr_MACRO(out, pOutStartPos);
	memcpy(pOutDest + iFirstSize, pOutStartPos, iSecondSize);
	MoveInOrOutPos_MACRO(out, iSecondSize);
	iOutPos_ = out;

	return iFirstSize + iSecondSize;
}

// 해당함수는 Dequeue와 거의 같지만 복사만 수행하고 front의 위치가 바뀌지 않는다. 
int RingBuffer::Peek(char* pOutTarget, int iSizeToPeek)
{
	int iUseSize;
	int iDirectPeekSize;
	int iFirstSize;
	int iSecondSize;
	char* pPeekStartPos;

	int in = iInPos_;
	int out = iOutPos_;

	GetUseSize_MACRO(in, out, iUseSize);
	if (iSizeToPeek > iUseSize)
	{
		// 들어있는 데이터보다 많은 데이터를 읽으려고 하면 그냥 반환한다.
		return 0;
	}

	GetDirectDequeueSize_MACRO(in, out, iDirectPeekSize);
	if (iDirectPeekSize > iSizeToPeek) // 잘려서 두번에 걸쳐서 복사
		iFirstSize = iSizeToPeek;
	else // 한번에 복사
		iFirstSize = iDirectPeekSize;

	GetOutStartPtr_MACRO(out, pPeekStartPos);
	memcpy(pOutTarget, pPeekStartPos, iFirstSize);

	iSecondSize = iSizeToPeek - iFirstSize;
	if (iSecondSize <= 0)
		return iFirstSize;

	memcpy(pOutTarget + iFirstSize, Buffer_, iSecondSize);
	return iFirstSize + iSecondSize;
}

int RingBuffer::PeekAt(char* pOutTarget, int iOut, int iSizeToPeek)
{
	int iUseSize;
	int iDirectPeekSize;
	int iFirstSize;
	int iSecondSize;
	char* pPeekStartPos;

	int in = iInPos_;
	GetUseSize_MACRO(in, iOut, iUseSize);
	if (iSizeToPeek > iUseSize)
	{
		// 들어있는 데이터보다 많은 데이터를 읽으려고 하면 그냥 반환한다.
		return 0;
	}

	GetDirectDequeueSize_MACRO(in, iOut, iDirectPeekSize);
	if (iDirectPeekSize > iSizeToPeek) // 잘려서 두번에 걸쳐서 복사
		iFirstSize = iSizeToPeek;
	else // 한번에 복사
		iFirstSize = iDirectPeekSize;

	GetOutStartPtr_MACRO(iOut, pPeekStartPos);
	memcpy(pOutTarget, pPeekStartPos, iFirstSize);

	iSecondSize = iSizeToPeek - iFirstSize;
	if (iSecondSize <= 0)
		return iFirstSize;

	memcpy(pOutTarget + iFirstSize, Buffer_, iSecondSize);
	return iFirstSize + iSecondSize;
}

void RingBuffer::ClearBuffer(void)
{
	iInPos_ = iOutPos_ = 0;
}


// 기능 : rear_ + 1부터 큐의 맨앞으로 이동하지 않고 Enqueue가능한 최대 크기를 반환함.
// 즉 rear+ 1부터 front_ - 1까지의 거리 혹은 rear_ + 1부터 
int RingBuffer::DirectEnqueueSize(void)
{
	int iRet;
	int in = iInPos_;
	int out = iOutPos_;
	GetDirectEnqueueSize_MACRO(in,out,iRet);
	return iRet;
}

int RingBuffer::DirectDequeueSize(void)
{
	int iRet;
	int in = iInPos_;
	int out = iOutPos_;
	GetDirectDequeueSize_MACRO(in, out, iRet);
	return iRet;
}

// sizeToMove만큼 rear_를 이동시키고 현재 rear_를 반환함.
int RingBuffer::MoveInPos(int iSizeToMove)
{
	MoveInOrOutPos_MACRO(iInPos_, iSizeToMove);
	return iInPos_;
}

// sizeToMove만큼 front_이동시키고 현재 rear_를 반환함.
int RingBuffer::MoveOutPos(int iSizeToMove)
{
	int OutPrev = iOutPos_;
	MoveInOrOutPos_MACRO(iOutPos_, iSizeToMove);
	int OutAfter = iOutPos_;
	return iOutPos_;
}

char* RingBuffer::GetWriteStartPtr(void)
{
	char* pRet;
	int in = iInPos_;
	GetInStartPtr_MACRO(in,pRet);
	return pRet;
}

char* RingBuffer::GetReadStartPtr(void)
{
	char* pRet;
	int out = iOutPos_;
	GetOutStartPtr_MACRO(out, pRet);
	return pRet;
}



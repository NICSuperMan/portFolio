#pragma once

constexpr int BUFFER_SIZE = 6000;
constexpr int ACTUAL_SIZE = BUFFER_SIZE + 1;

#define GetDirectEnqueueSize_MACRO(in,out,iRet) do\
{\
if(in >= out){\
	if(out > 0){\
		iRet = ACTUAL_SIZE - in;\
	}else{\
		iRet = BUFFER_SIZE - in;\
}}\
else{\
	iRet = out - in - 1;\
}\
}while(0)\

#define GetDirectDequeueSize_MACRO(in,out,iRet) do{\
if(in >= out){\
	iRet = in - out;\
}\
else{\
	iRet = ACTUAL_SIZE - out;\
}\
}while(0)\


#define GetUseSize_MACRO(in,out,iRet) do\
{\
	if(in >= out){\
		iRet = in - out;\
	}\
	else{\
		iRet = ACTUAL_SIZE - out + in;\
	}\
}while(0)\

#define GetFreeSize_MACRO(in,out,iRet) do\
{\
int iUseSize;\
GetUseSize_MACRO(in,out,iUseSize);\
iRet = BUFFER_SIZE - iUseSize;\
}while(0)\


#define GetInStartPtr_MACRO(in,iRet) do{\
iRet = (Buffer_ + in);\
}while(0)\

#define GetOutStartPtr_MACRO(out,iRet) do{\
iRet = (Buffer_ + out);\
}while(0)\

#define GetOutStartPtr_MACRO(out,iRet) do{\
iRet = (Buffer_ + out);\
}while(0)\

#define MoveInOrOutPos_MACRO(iPos,iMoveSize) do{\
iPos = (iPos + iMoveSize) % (ACTUAL_SIZE);\
}while(0);\

class RingBuffer
{
public:
	RingBuffer(void);
	~RingBuffer(void);
	int GetUseSize(void);
	int GetFreeSize(void);
	int Enqueue(const char* pSource, int iSizeToPut);
	int Dequeue(char* pOutDest, int iSizeToOut);
	int Peek(char* pOutTarget, int iSizeToPeek);
	int PeekAt(char* pOutTarget, int iOut, int iSizeToPeek);
	void ClearBuffer(void);
	int DirectEnqueueSize(void);
	int DirectDequeueSize(void);
	int MoveInPos(int iSizeToMove);
	int MoveOutPos(int iSizeToMove);
	char* GetWriteStartPtr(void);
	char* GetReadStartPtr(void);
	int iOutPos_;
	int iInPos_;
	char Buffer_[ACTUAL_SIZE];
};

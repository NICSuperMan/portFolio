#include <WinSock2.h>
#include "LanSession.h"

BOOL LanSession::Init(SOCKET clientSock, ULONGLONG ullClientID, SHORT shIdx)
{
    sock_ = clientSock;
    bSendingInProgress_ = FALSE;
    bSendingAtWorker_ = FALSE;
    InterlockedExchange(&id_, ((ullClientID << 16) ^ shIdx));
    lastRecvTime = GetTickCount64();
    bDisconnectCalled_ = FALSE;
    lSendBufNum_ = 0;
    recvRB_.ClearBuffer();
    return TRUE;
}

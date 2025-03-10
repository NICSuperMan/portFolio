#include <WinSock2.h>
#include "RingBuffer.h"
#include "GameSession.h"


BOOL GameSession::Init(SOCKET clientSock, ULONGLONG counter, SHORT idx)
{
    sock_ = clientSock;
    bSendingInProgress_ = FALSE;
    id_ = (counter << 16) ^ idx;
    bDisconnectCalled_ = FALSE;
    lSendBufNum_ = 0;
    bLogin_ = FALSE;
    recvRB_.ClearBuffer();
    return TRUE;
}

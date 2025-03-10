#include <WinSock2.h>
#include "LoginServer.h"
#include "LoginContent.h"
#include "CommonProtocol.h"
#include "QueryFactory.h"
#include "Packet.h"
#include "Parser.h"
#include "Assert.h"
#include "Logger.h"
#include "RedisClientWrapper.h"
#include "LoginPlayer.h"
#pragma comment(lib,"libmysql.lib")

using namespace cpp_redis;
constexpr int SESSION_KEY_LEN = 64;

void MAKE_CS_LOGIN_RES_LOGIN(ULONGLONG id, INT64 accountNo, BYTE status, WCHAR* pID, WCHAR* pNickName, WCHAR* pGameServerIP, SHORT gamePort, WCHAR* pChatServerIP, SHORT chatServerPort, SmartPacket& sp)
{
    *sp << (WORD)en_PACKET_CS_LOGIN_RES_LOGIN << accountNo << status;
    sp->PutData((char*)pID, 20 * sizeof(WCHAR));
    sp->PutData((char*)pNickName, 20 * sizeof(WCHAR));
    sp->PutData((char*)pGameServerIP, 16 * sizeof(WCHAR));
    *sp << gamePort;
    sp->PutData((char*)pChatServerIP, 16 * sizeof(WCHAR));
    *sp << chatServerPort;
}


LoginContent::LoginContent(LoginServer* pLoginServer)
    :ParallelContent{ pLoginServer }, pLoginServer_{ pLoginServer }
{
}

LoginContent::~LoginContent()
{
}

void LoginContent::OnEnter(void* pPlayer)
{
}

void LoginContent::OnLeave(void* pPlayer)
{
}

void LoginContent::OnRecv(Packet* pPacket, void* pPlayer)
{
    LoginPlayer* pLoginPlayer = (LoginPlayer*)pPlayer;

    WORD Type;
    INT64 accountNo;

    *pPacket >> Type;
    if (Type != en_PACKET_CS_LOGIN_REQ_LOGIN)
    {
        LOG(L"ERROR", SYSTEM, TEXTFILE, L"OnRecv PacketTypeErr Type is Not en_PACKET_CS_LOGIN_REQ_LOGIN, Type : %d", Type);
        __debugbreak();
    }

    *pPacket >> accountNo;
    char* pSessionKey = pPacket->GetPointer(SESSION_KEY_LEN);

    // 회원번호를 키로 DB에서 인증토큰 읽어오기
    QueryFactory::GetInstance()->MAKE_QUERY(
        "SELECT sessionkey FROM accountdb.sessionkey WHERE accountno = %d", accountNo);
    MYSQL_RES_PTR readTokenRet = QueryFactory::GetInstance()->ExecuteReadQuery();

    MYSQL_ROW sqlRow;
    sqlRow = mysql_fetch_row(&*readTokenRet);
    if (!sqlRow)
    {
        LOG(L"ERROR", SYSTEM, TEXTFILE, L"mysql_fetch_row Error At Onrecv, Failed To Get SessionKey");
        __debugbreak();
    }

    // 레디스에 인증토큰을 쓰기
    client* pClient = GetRedisClient();
    const std::string& acNoStr = std::to_string(accountNo);
    pClient->set(acNoStr, std::string{ pSessionKey ,SESSION_KEY_LEN });

    // 레디스에 만료시간 설정
    pClient->expire(acNoStr, 300);
    pClient->sync_commit();

    WCHAR ID[20];
    WCHAR NICK[20];

    QueryFactory::GetInstance()->MAKE_QUERY("SELECT userid,usernick FROM accountdb.account WHERE accountno = %d", accountNo);
    MYSQL_RES_PTR readAccountInfoRet = QueryFactory::GetInstance()->ExecuteReadQuery();
    sqlRow = mysql_fetch_row(&*readAccountInfoRet);

    size_t ret;
    ASSERT_NOT_ZERO(mbstowcs_s(&ret, ID, _countof(ID), sqlRow[0], SIZE_MAX));
    ASSERT_NOT_ZERO(mbstowcs_s(&ret, NICK, _countof(NICK), sqlRow[1], SIZE_MAX));

    WCHAR LOOPBACKIP[16]{ L"127.0.0.1" };
    WCHAR onePointOne[16]{ L"10.0.1.1" };
    WCHAR twoPointOne[16]{ L"10.0.2.1" };

    // 사용자에게 로그인 승인
    SmartPacket sp = PACKET_ALLOC(Net);
    if (pLoginServer_->bLoopBackTest_ == 1)
    {
        MAKE_CS_LOGIN_RES_LOGIN(pLoginPlayer->sessionID, accountNo, 1, ID, NICK, LOOPBACKIP, pLoginServer_->ChatServerPort_, LOOPBACKIP, pLoginServer_->ChatServerPort_, sp);
    }
    else
    {
        const WCHAR* pIP = pGameServer_->GetIp(pGameServer_->GetSessionID(pPlayer));
        const USHORT port = pGameServer_->GetPort(pGameServer_->GetSessionID(pPlayer));
        if (0 == wcscmp(pIP, L"10.0.1.2"))
        {
            // 로그인 성공 통지 패킷 생성
            MAKE_CS_LOGIN_RES_LOGIN(pLoginPlayer->sessionID, accountNo, 1, ID, NICK, onePointOne,
                pLoginServer_->ChatServerPort_, onePointOne, pLoginServer_->ChatServerPort_, sp);
        }
        else
        {
            MAKE_CS_LOGIN_RES_LOGIN(pLoginPlayer->sessionID, accountNo, 1, ID, NICK, twoPointOne, pLoginServer_->ChatServerPort_, twoPointOne, pLoginServer_->ChatServerPort_, sp);
        }
    }
    // 클라에게 전송
    pGameServer_->SendPacket(pLoginPlayer->sessionID, sp.GetPacket());
    InterlockedIncrement(&pLoginServer_->authTPS_);
}

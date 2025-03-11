#include <Winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <queue>
#include <unordered_map>
#include "Update.h"
#include "LoginDummy.h"
#include "ChatDummy.h"
#include "Player.h"
#include "RecvJob.h"
#include "MemoryPool.h"
#include <Common/CommonProtocol.h>
#include "Lyrics.h"
#include "Monitor.h"
#include "MakePacket.h"
#pragma comment(lib,"MemoryPool.lib")

#define VK_S 0x53
#define VK_A 0x41 
#define VK_W 0x57
#define VK_T 0x54
#define VK_R 0x52
#define VK_Q 0x51

extern LoginDummy* g_pLoginDummy;
extern ChatDummy* g_pChatDummy;
extern CLockFreeQueue<RecvJob*> g_msgQ;
extern CTlsObjectPool<RecvJob, true> g_jobPool;

int g_maxSession;
int g_disconnectedNum;

int g_loginConnectTotal;
int g_loginDownClient;
int g_loginConnectFail;

int g_duplicateConnectFail;

int g_chatConnectTotal;
int g_chatDownClient;
int g_chatConnectFail;

constexpr int RAND_ATTACK = 10;
constexpr int RAND_DUPLICATE_LOGIN = 30;
constexpr int RAND_TIMEOUT = 10;

int g_randConnect;
int g_randDisconnect;
int g_randContents;
int g_delayAction;
int g_delayLogin;
int g_replyWait;

MEMORYPOOL g_playerPool;

std::unordered_map<ULONGLONG, Player*> g_loginPlayerMap;
std::queue<Player*> g_moveQ;
std::unordered_map<ULONGLONG, Player*> g_chatConnectPlayerMap;
std::unordered_map<ULONGLONG, Player*> g_chatLoginPlayerMap;
std::stack<const AccountInfo*> g_NonConnectedStack;

int g_loopNum;
int g_first;
DWORD g_timeForSecond;
int g_ContentsRand;
DWORD g_timeForAvgReset;

// 같은 ChatMap안에서 
int g_chatConnectNum;
int g_chatLoginNum;

DWORD g_rttMin;
DWORD g_rttMax;
DWORD g_rttNum;
DWORD g_rttTotal;
float g_rttAvg;

bool g_bShutDown;
bool g_bPlay;
bool g_bConnectStop;
bool g_bDuplicateLoginOn;
bool g_bTestTimeOut;

int g_duplicateLoginTPS;
int g_sessionTimeOutTPS;
int g_loginTimeOutTPS;

DWORD g_loginTimeOutTotal;
DWORD g_loginTimeOutNum;

DWORD g_sessionTimeOutTotal;
DWORD g_sessionTimeOutNum;


// 평균은 1초에 한번구함
__forceinline void CaculateRTT(DWORD curRecvTime, DWORD lastSendTime)
{
	DWORD RTT = curRecvTime - lastSendTime;
	g_rttMin = min(g_rttMin, RTT);
	g_rttMax = max(g_rttMax, RTT);
	++g_rttNum;
	g_rttTotal += RTT;
}

__forceinline void RefreshLoginTimoutTotal(DWORD disconnectTime, DWORD loginTime)
{
	g_loginTimeOutTotal += (disconnectTime - loginTime);
	++g_loginTimeOutNum;
}

__forceinline void RefreshSessionTimoutTotal(DWORD disconnectTime, DWORD connectTime)
{
	g_sessionTimeOutTotal += (disconnectTime - connectTime);
	++g_sessionTimeOutNum;
}

void RecvLoginJob(RecvJob* pJob);
void RecvChatJob(RecvJob* pJob);
void RecvChatMessage(Packet* pPacket, ULONGLONG sessionID);
void SendRandomChatMessage(Player* pPlayer);


void UpdateLoop(int maxSession, int randConnect, int randDisconnect, int randContents, int delayAction, int delayLogin, const WCHAR* pIDConfigFile, const WCHAR* pLyricsConfigFile)
{
	srand((unsigned)time(nullptr));
	g_bPlay = true;
	MonitorInit();
	AccountInfo::Init(pIDConfigFile);
	LyRics::Init(pLyricsConfigFile);

	for (int i = 0; i < maxSession; ++i)
	{
		g_NonConnectedStack.push(AccountInfo::Alloc());
	}

	g_playerPool = CreateMemoryPool(sizeof(Player), Player::MAX_PLAYER_NUM);
	g_maxSession = g_disconnectedNum = maxSession;
	g_randConnect = randConnect;
	g_randDisconnect = randDisconnect;
	g_randContents = randContents;
	g_delayAction = delayAction;
	g_delayLogin = delayLogin;

	g_first = timeGetTime();
	g_timeForAvgReset = g_timeForSecond = g_first;
	g_loopNum = 0;

	while (!g_bShutDown)
	{
		Update();
		DWORD curTime = timeGetTime();
		if (curTime >= g_timeForSecond + 1000)
		{
			Monitor();
			g_timeForSecond += 1000;
			g_loopNum = 0;

			if (curTime >= g_timeForAvgReset + 1000 * 60 * 3)
			{
				g_rttNum = 0;
				g_rttTotal = 0;
				g_loginTimeOutTotal = 0;
				g_loginTimeOutNum = 0;
				g_sessionTimeOutTotal = 0;
				g_sessionTimeOutNum = 0;
				g_timeForAvgReset += 1000 * 60;
			}

			if (GetAsyncKeyState(VK_S) & 0x0001)
			{
				g_bPlay = !g_bPlay;
			}

			if (GetAsyncKeyState(VK_A) & 0x0001)
			{
				g_bConnectStop = !g_bConnectStop;
			}

			if (GetAsyncKeyState(VK_W) & 0x0001)
			{
				g_bDuplicateLoginOn = !g_bDuplicateLoginOn;
			}

			if (GetAsyncKeyState(VK_R) & 0x0001)
			{
				g_bTestTimeOut= !g_bTestTimeOut;
			}

			if (GetAsyncKeyState(VK_Q) & 0x0001)
			{
				g_bShutDown = true;
			}
		}
		Sleep(0);
		++g_loopNum;
	}
	// 셧다운 절차
}

void Update()
{
	while (1)
	{
		const auto& opt = g_msgQ.Dequeue();
		if (!opt.has_value())
			break;

		RecvJob* pJob = opt.value();
		switch (pJob->serverType_)
		{
		case SERVERTYPE::LOGIN:
			RecvLoginJob(pJob);
			break;
		case SERVERTYPE::CHAT:
			RecvChatJob(pJob);
			break;
		default:
			__debugbreak();
			break;
		}

		if (pJob->pPacket_ && pJob->pPacket_->DecrementRefCnt() == 0)
			PACKET_FREE(pJob->pPacket_);

		g_jobPool.Free(pJob);
	}

	// Stop일때는 받은메시지만 전부 해석해서 ReplyWait만 감소시키고 실질적인 메시지 전송은 안한다.
	if (!g_bPlay) return;

	// 더미에 설정된 확률대로 Connect 시도
	int temp = g_disconnectedNum;
	if (!g_bConnectStop)
	{
		for (int i = 0; i < temp; ++i)
		{
			if (rand() % 100 < g_randConnect)
			{
				g_pLoginDummy->Connect(false, &g_pLoginDummy->sockAddr_);
				--g_disconnectedNum;
			}
		}
	}


	for (auto pair : g_loginPlayerMap)
	{
		Player* pPlayer = pair.second;
		if((pPlayer->field_ & DISCONNECT_TARGET) == DISCONNECT_TARGET)
			continue;

		ULONGLONG curTime = timeGetTime();
		switch (pPlayer->state_)
		{
		case STATE::LOGIN_CONNECT_SUCCESS:
		{
			if (curTime < pPlayer->lastRecvTime_ + g_delayLogin)
				break;

			// delay login 만큼의 시간이 흐르면 로그인 패킷 보내기
			SmartPacket sp = PACKET_ALLOC(Net);
			MAKE_CS_LOGIN_REQ_LOGIN(pPlayer->pInfo_->accountNo, pPlayer->pInfo_->SessionKey, sp);
			g_pLoginDummy->SendPacket(pPlayer->sessionId_, sp);
			pPlayer->lastSendTime_ = timeGetTime();
			pPlayer->state_ = STATE::CS_LOGIN_REQ_LOGIN_SENT;
			++g_replyWait;
			break;
		}

		case STATE::CS_LOGIN_REQ_LOGIN_SENT:
		case STATE::LOGIN_TRY_DISCONNECT:
			break;

		default:
			__debugbreak();
			break;
		}

	}

	ULONGLONG curTime = timeGetTime();
	for (auto pair : g_chatConnectPlayerMap)
	{
		Player* pPlayer = pair.second;

		if (pPlayer->state_ != STATE::CHAT_CONNECT_SUCCESS && pPlayer->state_ != STATE::CS_CHAT_REQ_LOGIN_SENT)
			__debugbreak();

		if ((pPlayer->field_ & SESSION_TIMEOUT_TARGET) == SESSION_TIMEOUT_TARGET) // 로그인 요청 패킷 보내지 않고 세션상태를 유지하는 악성유저에 대해서 타임아웃 테스트 중이라면 건너뜀
			continue;

		if (pPlayer->state_ == STATE::CS_CHAT_REQ_LOGIN_SENT)
			continue;

		if (curTime < pPlayer->lastRecvTime_ + g_delayLogin)
			continue;

		// delay login 만큼의 시간이 흐르면 로그인 패킷 보내기
		SmartPacket sp = PACKET_ALLOC(Net);
		MAKE_CS_CHAT_REQ_LOGIN(pPlayer->pInfo_->accountNo, pPlayer->pInfo_->ID, pPlayer->pInfo_->Nick, pPlayer->pInfo_->SessionKey, sp);
		g_pChatDummy->SendPacket(pPlayer->sessionId_, sp);
		pPlayer->lastSendTime_ = timeGetTime();
		pPlayer->state_ = STATE::CS_CHAT_REQ_LOGIN_SENT;
		++g_replyWait;
	}

	curTime = timeGetTime();
	for (auto pair : g_chatLoginPlayerMap)
	{
		Player* pPlayer = pair.second;

		switch (pPlayer->state_)
		{
		case STATE::CS_CHAT_RES_LOGIN_RECV:
		case STATE::CS_CHAT_RES_SECTOR_MOVE_RECV:
		case STATE::CS_CHAT_RES_MESSAGE_RECV:
		{
			// 더미클라에서 Disconnect를 호출해도 IOCP에서 비동기적으로 끊기 때문에 끊길 예정인 클라가 아직 Map에 존재할수잇음
			if ((pPlayer->field_ & DISCONNECT_TARGET) == DISCONNECT_TARGET)
				continue;

			// 이중로그인 상태, 로그인 타임아웃 테스트 클라로 선정되엇다면 더 이상 메시지를 보내지 않는다
			if ((pPlayer->field_ & (DUPLICATE_TARGET | LOGIN_TIMOUT_TARGET)) != 0)
				continue;

			// 확률 계산후 메시지를 보내기로 결정된 플레이어에 대해서 DELAY_ACTION만큼의 시간이 흘럿는지 체크후 흘럿으면 메시지 보냄
			if ((pPlayer->field_ & RESERVE_SEND_MESSAGE) == RESERVE_SEND_MESSAGE)
			{
				if (curTime < pPlayer->lastRecvTime_ + g_delayAction)
					break;

				// 보낼 메시지 만들고 보내기
				SendRandomChatMessage(pPlayer);
				pPlayer->lastSendTime_ = timeGetTime();
				pPlayer->field_ &= (~RESERVE_SEND_MESSAGE);
				++g_replyWait;
				break;
			}

			// RAND_CONNTENTS에 당첨됫는지 확률계산후 당첨이라면 메시지 보내기 예약, DEALY_ACTION만큼의 시간이 흐르면 메시지 전송
			if (rand() % 100 < g_randContents)
				pPlayer->field_ |= RESERVE_SEND_MESSAGE;

			break;
		}

		// 메시지를 보낸 상태에서는 핑퐁이라서 사실상 할수있는게 없다.
		case STATE::CS_CHAT_REQ_LOGIN_SENT:
		case STATE::CS_CHAT_REQ_SECTOR_MOVE_SENT:
		case STATE::CS_CHAT_REQ_MESSAGE_SENT:
		case STATE::CS_CHAT_REQ_HEARTBEAT_SENT:
		case STATE::CHAT_TRY_DISCONNECT:
			break;

		default:
			__debugbreak();
			break;
		}

		if ((pPlayer->field_ & LOGIN_TIMOUT_TARGET) == LOGIN_TIMOUT_TARGET) // 정상유저에 대한 타임아웃 검증 대상 클라라면 하트비트를 보내지 않아야한다
			break;

		// 채팅서버 로그인 인증유저는 30초에 한번 하트비트 보내기
		if (curTime >= pPlayer->lastHeartbeatSendtime_ + 3000 * 10)
		{
			SmartPacket sp = PACKET_ALLOC(Net);
			*sp << (WORD)en_PACKET_CS_CHAT_REQ_HEARTBEAT;
			g_pChatDummy->SendPacket(pPlayer->sessionId_, sp);
			pPlayer->lastHeartbeatSendtime_ = timeGetTime();
		}
	}
}


void RecvLoginJob(RecvJob* pJob)
{
	switch (pJob->type_)
	{
	case JOBTYPE::CONNECT_SUCCESS:
	{
		Player* pPlayer = (Player*)AllocMemoryFromPool(g_playerPool);
		pPlayer->serverType_ = SERVERTYPE::LOGIN;
		pPlayer->bDuplicateNew_ = false;
		pPlayer->sessionId_ = pJob->sessionId_;
		pPlayer->lastRecvTime_ = timeGetTime();
		pPlayer->state_ = STATE::LOGIN_CONNECT_SUCCESS;
		pPlayer->field_ = INITIAL_FIELD;

		do
		{
			// 이중로그인 모드가 켜져잇고 동시에 확률을 만족하지 않으면 이중로그인 진행 X
			if (!(g_bDuplicateLoginOn && (rand() % 100 < RAND_DUPLICATE_LOGIN)))
			{
				pPlayer->pInfo_ = g_NonConnectedStack.top();
				g_NonConnectedStack.pop();
				break;
			}

			// Disconnect 예정, 타임아웃 테스트,이중로그인 등의 현재 시점에서 어떠한 테스트 대상 유저도 아닌 유저중에서 선정한다
			const auto& iter = std::find_if(g_chatLoginPlayerMap.begin(), g_chatLoginPlayerMap.end(), [](auto pair) {return IS_VALID_FIELD_FOR_TEST(pair.second->field_); });
			if (iter == g_chatLoginPlayerMap.end())
			{
				pPlayer->pInfo_ = g_NonConnectedStack.top();
				g_NonConnectedStack.pop();
				break;
			}

			// 이미 채팅서버에 로그인해서 이중로그인 테스트 대상으로 선정된 유저에 대해 속성필드설정 및 해당 유저의 정보를 지금 로그인 서버에 접속하는 유저에게 복사
			iter->second->field_ |= DUPLICATE_TARGET;
			pPlayer->pInfo_ = iter->second->pInfo_;
			++g_duplicateLoginTPS;
			pPlayer->bDuplicateNew_ = true;
		} while (0);

		g_loginPlayerMap.insert(std::make_pair(pJob->sessionId_, pPlayer));
		++g_loginConnectTotal;
		break;
	}
	case JOBTYPE::RECV_MESSAGE:
	{
		--g_replyWait;
		Packet* pPacket = pJob->pPacket_;

		Player* pPlayer = g_loginPlayerMap.find(pJob->sessionId_)->second;
		pPlayer->lastRecvTime_ = timeGetTime();
		CaculateRTT(pPlayer->lastRecvTime_, pPlayer->lastSendTime_);

		WORD type;
		BYTE status;
		INT64 accountNo;
		*pPacket >> type;

		if (type != en_PACKET_CS_LOGIN_RES_LOGIN)
			__debugbreak();

		*pPacket >> accountNo >> status;

		if (status != 1)
			__debugbreak();

		if (pPlayer->pInfo_->accountNo != accountNo)
			__debugbreak();

		WCHAR* pID = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
		WCHAR* pNick = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);

		USHORT gamePort;
		USHORT chatPort;

		WCHAR* pGameIP = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * 16);
		*pPacket >> gamePort;

		WCHAR* pChatIP = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * 16);
		*pPacket >> chatPort;

		// 채팅서버 Connect할 SOCKADDR_IN 만들기
		ZeroMemory(&pPlayer->sockAddr_, sizeof(SOCKADDR_IN));
		pPlayer->sockAddr_.sin_family = AF_INET;
		InetPtonW(AF_INET, pChatIP, &pPlayer->sockAddr_.sin_addr);
		pPlayer->sockAddr_.sin_port = htons(chatPort);

		pPlayer->state_ = STATE::LOGIN_TRY_DISCONNECT;
		pPlayer->field_ |= DISCONNECT_TARGET;
		g_pLoginDummy->Disconnect(pJob->sessionId_);
		break;
	}
	case JOBTYPE::ON_RELEASE:
	{
		auto iter = g_loginPlayerMap.find(pJob->sessionId_);
		Player* pPlayer = iter->second;
		if (pPlayer->state_ == STATE::LOGIN_TRY_DISCONNECT) // 성공메시지를 수신해서 클라에서 정상적으로 Disconnect 한 경우
		{
			pPlayer->state_ = STATE::CHAT_CONNECT_TRY;
			g_moveQ.push(pPlayer);
			g_pChatDummy->Connect(false, &pPlayer->sockAddr_);
		}
		else // 로그인 인증 성공 메시지를 서버에게서 받지 못햇는데 연결이 끊긴경우
		{
			++g_loginDownClient;
			++g_disconnectedNum;
			g_NonConnectedStack.push(pPlayer->pInfo_);
			RetMemoryToPool(g_playerPool, pPlayer);
		}
		g_loginPlayerMap.erase(iter);
		break;
	}
	case JOBTYPE::CONNECT_FAIL:
	{
		++g_loginConnectFail;
		break;
	}
	case JOBTYPE::ALL_RECONNECT_FAIL:
	{
		++g_disconnectedNum;
		break;
	}
	default:
		__debugbreak();
		break;
	}
}

void RecvChatJob(RecvJob* pJob)
{
	switch (pJob->type_)
	{
	case JOBTYPE::CONNECT_SUCCESS:
	{
		Player* pPlayer = g_moveQ.front();
		g_moveQ.pop();
		pPlayer->serverType_ = SERVERTYPE::CHAT;
		pPlayer->sessionId_ = pJob->sessionId_;
		pPlayer->lastRecvTime_ = timeGetTime();
		pPlayer->lastHeartbeatSendtime_ = timeGetTime();
		pPlayer->state_ = STATE::CHAT_CONNECT_SUCCESS;
		pPlayer->field_ &= ~DISCONNECT_TARGET;

		// 타임아웃 설정된 경우 일정확률 만족시 타임아웃 클라로 설정
		// Connect 시점에서는 Duplicate Target일 수가 없음
		// 다만 로그인 세션 테스트클라로 이중로그인에서 나중에 들어오는 클라가 선정되면 로그인 이전에 타임아웃으로 서버로부터 끊길 위험이 있으므로 bDuplicateNew가 false인 클라에 한해서 선정
		if (g_bTestTimeOut && !pPlayer->bDuplicateNew_ && (rand() % 100 < RAND_TIMEOUT))
		{
			// 50 / 50의 확률로 세션 타임아웃인지 로그인 타임아웃인지 결정
			if (rand() % 2 == 0)
			{
				pPlayer->field_ |= SESSION_TIMEOUT_TARGET;
				++g_sessionTimeOutTPS;
			}
			else
			{
				pPlayer->field_ |= LOGIN_TIMOUT_TARGET;
				++g_loginTimeOutTPS;
			}
		}

		g_chatConnectPlayerMap.insert(std::make_pair(pJob->sessionId_, pPlayer));
		++g_chatConnectTotal;
		break;
	}
	case JOBTYPE::RECV_MESSAGE:
	{
		RecvChatMessage(pJob->pPacket_, pJob->sessionId_);
		break;
	}
	case JOBTYPE::ON_RELEASE:
	{
		++g_disconnectedNum;
		Player* pPlayer;
		auto connectIter = g_chatConnectPlayerMap.find(pJob->sessionId_);
		if (connectIter != g_chatConnectPlayerMap.end())
		{
			--g_chatConnectNum;
			pPlayer = connectIter->second;
			g_chatConnectPlayerMap.erase(connectIter);
			g_NonConnectedStack.push(pPlayer->pInfo_);

			pPlayer = connectIter->second;
			if ((pPlayer->field_ & SESSION_TIMEOUT_TARGET) == SESSION_TIMEOUT_TARGET)
			{
				RefreshSessionTimoutTotal(timeGetTime(), pPlayer->lastRecvTime_);
				--g_sessionTimeOutTPS;
			}
			else // login되지 않은 상태에서는 테스트 클라가 아니라면 더미 측에서 Disconnect가 먼저 호출될일이 없기에 STATE::CHAT_TRY_DISCONNECT가 될일이 없음
			{
				++g_chatDownClient;
			}
		}
		else
		{
			--g_chatLoginNum;
			auto loginIter = g_chatLoginPlayerMap.find(pJob->sessionId_);
			if (loginIter == g_chatLoginPlayerMap.end())
			{
				LOG(L"ERROR", ERR, TEXTFILE, L"chat Session Is Release But Neither Connect Or Login Map");
				__debugbreak();
			}
			pPlayer = loginIter->second;
			g_chatLoginPlayerMap.erase(loginIter);

			if ((pPlayer->field_ & DUPLICATE_TARGET) == DUPLICATE_TARGET)
			{
				--g_duplicateLoginTPS;
			}
			else
			{
				g_NonConnectedStack.push(pPlayer->pInfo_);
				if ((pPlayer->field_ & LOGIN_TIMOUT_TARGET) == LOGIN_TIMOUT_TARGET)
				{
					RefreshLoginTimoutTotal(timeGetTime(),pPlayer->lastRecvTime_);
					--g_loginTimeOutTPS;
				}
				else if (pPlayer->state_ != STATE::CHAT_TRY_DISCONNECT)
				{
					++g_chatDownClient;
				}
			}
		}

		RetMemoryToPool(g_playerPool, pPlayer);
		break;
	}
	case JOBTYPE::CONNECT_FAIL:
	{
		++g_chatConnectFail;
		break;
	}

	case JOBTYPE::ALL_RECONNECT_FAIL: //  채팅서버로의 재접속이 일정횟수 실패시 
	{
		++g_disconnectedNum;
		Player* pPlayer = g_moveQ.front();
		g_moveQ.pop();
		if (pPlayer->bDuplicateNew_)
		{
			++g_duplicateConnectFail;
		}

		g_NonConnectedStack.push(pPlayer->pInfo_);
		RetMemoryToPool(g_playerPool, pPlayer);
		break;
	}
	default:
		__debugbreak();
		break;
	}
}

void RecvChatMessage(Packet* pPacket, ULONGLONG sessionID)
{
	// 이중로그인 : 어느시점에 이중로그인 대상으로 변경될지 모르므로 무슨 메시지가 와도 이상하지 않음.
	// 타임아웃(세션) : CONNECT 성공시 선정여부가 결정되므로 메시지가 도착해서는 안됨
	// 타임아웃(로그인) : CONNECT 성공시 선정여부가 결정되고 인증을 위해서 로그인까지만 진행하기 때문에 RES_SECTOR_MOVE 혹은 RES_CHAT_MESSAGE는 도착해서는 안됨
	auto iter = g_chatConnectPlayerMap.find(sessionID);
	if (iter == g_chatConnectPlayerMap.end())
	{
		iter = g_chatLoginPlayerMap.find(sessionID);
		if (iter == g_chatLoginPlayerMap.end())
			__debugbreak();
	}

	Player* pPlayer = iter->second;

	WORD type;
	*pPacket >> type;
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_CHAT_RES_LOGIN:
	{
		pPlayer->lastRecvTime_ = timeGetTime();
		CaculateRTT(pPlayer->lastRecvTime_, pPlayer->lastSendTime_);

		// RES_MESSAGE의 경우에는 Disconnect를 호출하고 IOCnt가 0이되는 사이에 다른 주위섹터 클라로부터 온 메시지가 MessageQ에 Enqueue되어버릴 가능성이 잇기때문임
		if (pPlayer->state_ == STATE::CHAT_TRY_DISCONNECT)
		{
			LOG(L"ERROR", ERR, TEXTFILE, L"Call Disconnect But Recv RES_LOGIN");
			__debugbreak();
		}

		if (pPlayer->state_ != STATE::CS_CHAT_REQ_LOGIN_SENT)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Previous Send Message Is Not CS_CHAT_REQ_LOGIN");
			__debugbreak();
		}

		BYTE status;
		INT64 accountNo;

		*pPacket >> status >> accountNo;
		if (status != 1)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"AccountNo : %d, Login Status : %d, Not 1", accountNo, status);
			__debugbreak();
		}

		if (accountNo != pPlayer->pInfo_->accountNo)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"LoginPacket Send AccountNo : %d, Recv AccountNo : %d", pPlayer->pInfo_->accountNo, accountNo);
			__debugbreak();
		}

		pPlayer->state_ = STATE::CS_CHAT_RES_LOGIN_RECV;
		pPlayer->field_ |= LOGIN_ACTIVATE_AT_CHAT;
		g_chatConnectPlayerMap.erase(iter);
		g_chatLoginPlayerMap.insert(std::make_pair(sessionID, pPlayer));

		--g_chatConnectNum;
		++g_chatLoginNum;
		--g_replyWait;
		break;
	}
	case en_PACKET_CS_CHAT_RES_SECTOR_MOVE:
	{
		pPlayer->lastRecvTime_ = timeGetTime();
		CaculateRTT(pPlayer->lastRecvTime_, pPlayer->lastSendTime_);

		// RES_MESSAGE의 경우에는 Disconnect를 호출하고 IOCnt가 0이되는 사이에 다른 주위섹터 클라로부터 온 메시지가 MessageQ에 Enqueue되어버릴 가능성이 잇기때문임
		if (pPlayer->state_ == STATE::CHAT_TRY_DISCONNECT)
		{
			LOG(L"ERROR", ERR, TEXTFILE, L"Call Disconnect But Recv RES_SECTOR_MOVE");
			__debugbreak();
		}

		if (pPlayer->state_ != STATE::CS_CHAT_REQ_SECTOR_MOVE_SENT)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Previous Send Message Is Not CS_CHAT_REQ_SECTOR_MOVE");
			__debugbreak();
		}

		INT64 accountNo;
		WORD sectorX, sectorY;
		*pPacket >> accountNo;

		if (accountNo != pPlayer->pInfo_->accountNo)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"SectorMove Send AccountNo : % d, Recv AccountNo : % d", pPlayer->pInfo_->accountNo, accountNo);
			__debugbreak();
		}

		*pPacket >> sectorX >> sectorY;

		if (pPlayer->sectorX_ != sectorX || pPlayer->sectorY_ != sectorY)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"SectorMove Send X,Y(%d,%d) Recv X,Y(%d,%d)", pPlayer->sectorX_, pPlayer->sectorY_, sectorX, sectorY);
			__debugbreak();
		}

		pPlayer->state_ = STATE::CS_CHAT_RES_SECTOR_MOVE_RECV;
		--g_replyWait;
		break;
	}

	case en_PACKET_CS_CHAT_RES_MESSAGE: 
	{
		if((pPlayer->field_ & REGISTER_AT_SECTOR) == 0)
		{
			LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Player Is Not Registered At Sector But Chat Message Recved");
			__debugbreak();
		}

		INT64	accountNo;
		*pPacket >>  accountNo;
		WCHAR* pID = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::ID_LEN);
		WCHAR* pNick = (WCHAR*)pPacket->GetPointer(sizeof(WCHAR) * Player::NICK_NAME_LEN);
		WORD messageByte;
		*pPacket >> messageByte;
		WCHAR* pChat = (WCHAR*)pPacket->GetPointer(messageByte);

		if (accountNo == pPlayer->pInfo_->accountNo) // 본인이 보낸 메시지를 본인이 받은경우(검증대상)
		{
			pPlayer->lastRecvTime_ = timeGetTime();
			CaculateRTT(pPlayer->lastRecvTime_, pPlayer->lastSendTime_);

			if ((pPlayer->field_ & DUPLICATE_TARGET) == DUPLICATE_TARGET)
			{
				return;
			}

			// 본인이 보낸 메시지를 본인이 받앗는데 이전에 보낸 메시지가 채팅메시지 요청이 아닐때
			if (pPlayer->state_ != STATE::CS_CHAT_REQ_MESSAGE_SENT)
			{
				LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Previous Send Message Is Not CS_CHAT_REQ_MESSAGE", pPlayer->pInfo_->ID, pID);
				__debugbreak();
			}

			bool bWrong = false;
			if (0 != wcscmp(pID, pPlayer->pInfo_->ID))
			{
				LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Send ID : %s Recv ID : %s", pPlayer->pInfo_->ID, pID);
				__debugbreak();
				bWrong = true;
			}

			if (0 != wcscmp(pNick, pPlayer->pInfo_->Nick))
			{
				LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Send Nick : %s Recv Nick : %s", pPlayer->pInfo_->Nick, pNick);
				__debugbreak();
				bWrong = true;
			}

			if (messageByte / 2 != pPlayer->pLastSentChat->len)
			{
				LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Send Len : %d Recv Len : %d", pPlayer->pLastSentChat->len, messageByte / 2);
				__debugbreak();
				bWrong = true;
			}

			if (0 != wcsncmp(pChat, pPlayer->pLastSentChat->pLyrics, messageByte / 2))
			{
				LOG(L"RECV_PACKET_ERROR", ERR, TEXTFILE, L"Send Chat : %s Recv Chat : %s", pPlayer->pLastSentChat->pLyrics, pChat);
				__debugbreak();
				bWrong = true;
			}


			if (bWrong)
				__debugbreak();

			pPlayer->state_ = STATE::CS_CHAT_RES_MESSAGE_RECV;
			--g_replyWait;
		}
		else
			return;
		break;
	}
	default:
		__debugbreak();
		break;
	}

	// 메시지 수신시 일정 확률로 테스트 대상 선정 혹은 DISCONNECT
	// 이중로그인, 타임아웃, 어택 테스트중인 클라는 임의로 끊지않음, 대부분은 핑퐁이기 때문에 여기까지 오기전에 debugbreak가 걸리나 주위 유저에 의해 CHAT_RES_MESSAGE를 받는경우가 이에 해당됨
	if ((pPlayer->field_ & (DUPLICATE_TARGET | DISCONNECT_TARGET | SESSION_TIMEOUT_TARGET | LOGIN_TIMOUT_TARGET)) != 0)
		return;

	if (rand() % 100 < g_randDisconnect)
	{
		pPlayer->field_ |= DISCONNECT_TARGET;
		pPlayer->state_ = STATE::CHAT_TRY_DISCONNECT;
		g_pChatDummy->Disconnect(sessionID);
	}
}

void SendRandomChatMessage(Player* pPlayer)
{
	SmartPacket sp = PACKET_ALLOC(Net);
	if (((pPlayer->field_ & REGISTER_AT_SECTOR) == 0) || rand() % 5 < 1) // 섹터에 등록이 안되잇으면 무조건 섹터이동 보내기, 그게아니면 확률적결정
	{
		pPlayer->sectorX_ = rand() % 49 + 1;
		pPlayer->sectorY_ = rand() % 49 + 1;
		MAKE_CS_CHAT_REQ_SECTOR_MOVE(pPlayer->pInfo_->accountNo, pPlayer->sectorX_, pPlayer->sectorY_, sp);
		pPlayer->state_ = STATE::CS_CHAT_REQ_SECTOR_MOVE_SENT;
		pPlayer->field_ |= REGISTER_AT_SECTOR; // 여기서 미리등록해야 그사이에 RES_MESSAGE 다른 유저가 보낸 메시가 왓을때 섹터에는 등록이 되어있지 않은데 채팅메시지를 받앗다는 논리적 오류가 안생김
	}
	else
	{
		const LyRics* pLR = pPlayer->pLastSentChat = LyRics::Get(); // 나중에 본인에게 핑퐁올때 검증하려면 저장해둬야함
		MAKE_CS_CHAT_REQ_MESSAGE(pPlayer->pInfo_->accountNo, pLR->len, pLR->pLyrics, sp);
		pPlayer->state_ = STATE::CS_CHAT_REQ_MESSAGE_SENT;
	}
	g_pChatDummy->SendPacket(pPlayer->sessionId_, sp);
	pPlayer->lastSendTime_ = timeGetTime();
}

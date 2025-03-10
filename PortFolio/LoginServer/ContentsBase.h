#pragma once
#include "ThreadMessage.h"
#include "GameSession.h"

class GameServer;

class ContentsBase
{
protected:
	ContentsBase(const bool bSerial, GameServer* pGameServer)
		:pGameServer_{ pGameServer }, bSerial_{ bSerial } 
	{}

	virtual ~ContentsBase();
public:
	virtual void OnEnter(void* pPlayer) = 0;
	virtual void OnLeave(void* pPlayer) = 0;
	virtual void OnRecv(Packet* pPacket, void* pPlayer) = 0;
	virtual void WorkerHanlePacketAtRecvLoop(Packet* pPacket, GameSession* pSession) = 0;
	virtual void RequestFirstEnter(void* pPlayer) = 0;
	virtual void RequestEnter(const bool bPrevContentsIsSerialize, GameSession* pSession) = 0;
protected:
	virtual void ReleaseSession(GameSession* pSession) = 0;
	virtual void RegisterLeave(void* pPlayer, int nextContent) = 0;
public:
	static inline void RegisterContents(int contentsType, const ContentsBase* pContent)
	{
		if (contentsType >= arrayLength) __debugbreak();
		pArr_[contentsType] = const_cast<ContentsBase*>(pContent);
	}

	__forceinline static void SetContentsToFirst(int firstContentType)
	{
		pFirst = pArr_[firstContentType];
	}

	__forceinline static void FirstEnter(void* pPlayer)
	{
		const_cast<ContentsBase*>(pFirst)->RequestFirstEnter(pPlayer);
	}

	__forceinline static  ContentsBase* GetContentsPtr(int contentType)
	{
		return pArr_[contentType];
	}

	void ReleaseSession_AT_ONCE_NOT_CALL_ONLEAVE(GameSession* pSession);
private:
	static constexpr int arrayLength = 1000;
	static inline ContentsBase* pArr_[arrayLength];
	static inline const ContentsBase* pFirst;
	friend class GameServer;
protected:
	GameServer* pGameServer_;
	const bool bSerial_;
};

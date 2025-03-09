#pragma once
#include <queue>
#include "CTlsObjectPool.h"
#include "Packet.h"
#include "LoginChatServer.h"

struct JOB
{
public:
	JOB(ULONGLONG sessionId)
		:sessionId_{ sessionId }
	{}

	virtual void distributeJobToOrderQ() = 0;
	virtual void Excute(int order) = 0;
	static void FlushOrderJobQ(int order);

	ULONGLONG sessionId_;
	static inline std::queue<JOB*> orderJobQ_[4];
};

struct SECTOR_ADD_JOB : public JOB
{
public:
	SECTOR_ADD_JOB(Packet* pPacket, ULONGLONG sessionId, WORD addSectorX, WORD addSectorY);

	virtual void distributeJobToOrderQ() override;
	virtual void Excute(int order) override;

	BYTE order_;
	WORD addSectorX_;
	WORD addSectorY_;
	Packet* pPacket_;
public:
	static inline CTlsObjectPool<SECTOR_ADD_JOB, true> addJobPool_;
};

struct SECTOR_REMOVE_JOB : public JOB
{
	SECTOR_REMOVE_JOB(ULONGLONG sessionId, WORD removeSectorX, WORD removeSectorY);

	BYTE order_;
	WORD removeSectorX_;
	WORD removeSectorY_;

	virtual void distributeJobToOrderQ() override;
	virtual void Excute(int order) override;

	static inline CTlsObjectPool<SECTOR_REMOVE_JOB, true> removeJobPool_;
};

struct BROADCAST_JOB : public JOB
{
	BROADCAST_JOB(Packet* pPacket, ULONGLONG sessionId, WORD sectorX, WORD sectorY);

	virtual void distributeJobToOrderQ();
	virtual void Excute(int order) override;

	WORD sectorX_;
	WORD sectorY_;
	int belowThan4WhenOneOrder_;
	bool orderArr_[4]{ false };
	LONG refCnt_;
	Packet* pPacket_;
	static inline CTlsObjectPool<BROADCAST_JOB, true> broadCastJobPool_;
};

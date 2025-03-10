#pragma once
struct GameSession;

enum class en_MsgType
{
	ENTER,
	RELEASE
};

struct InterContentsMessage
{
	const en_MsgType msgType_;
	GameSession* pSession_;
	const int nextContent_;

	InterContentsMessage(en_MsgType msgType, GameSession* pSession, int nextContent = -1)
		:msgType_{ msgType }, pSession_{ pSession }, nextContent_{ nextContent }
	{}
};

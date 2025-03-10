#pragma once
#include <cpp_redis/cpp_redis>
#include <Windows.h>


struct RedisClientWrapper
{
	cpp_redis::client* pClient_;
	static inline CRITICAL_SECTION cs;
	RedisClientWrapper()
	{
		pClient_ = new cpp_redis::client ;
		pClient_->connect();
	}

	~RedisClientWrapper()
	{
		//끊기기 전까지 블라킹된다
		// 자꾸 소멸자에서 블라킹되서 일단 연결만 끊고 종료
		pClient_->disconnect(true);
	}
};

inline cpp_redis::client* GetRedisClient()
{
	thread_local RedisClientWrapper client;
	return client.pClient_;
}

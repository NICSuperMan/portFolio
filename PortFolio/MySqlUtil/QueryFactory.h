#pragma once
#include <windows.h>
#include <mutex>
#include "mysql.h"

using MYSQL_RES_DELETER = void(_stdcall*)(MYSQL_RES*);
using MYSQL_RES_PTR = std::unique_ptr<MYSQL_RES, MYSQL_RES_DELETER>;

class QueryFactory
{
private:
	struct BufInfo
	{
		BufInfo();
		~BufInfo();
		char* pBuf;
		char* nextPos;
		int size;
		MYSQL* connection;
		MYSQL conn;
		__forceinline void Clear()
		{
			this->nextPos = this->pBuf;
		}
	};
	static constexpr int INITIAL_SIZE = 500;
	BufInfo* GetBufInfo();
	QueryFactory();
	void Resize(BufInfo* pBI, char* end);
	static inline std::once_flag flag;
public:
	MYSQL_RES_PTR ExecuteReadQuery();
	int ExcuteWriteQuery();
	const char* MAKE_QUERY(const char* pStr, ...);

	__forceinline void Clear()
	{
		BufInfo* pBI = GetBufInfo();
		pBI->nextPos = pBI->pBuf;
	}

	static inline QueryFactory* pInstance;
	static QueryFactory* GetInstance()
	{
		std::call_once(QueryFactory::flag, []() {
			InitializeCriticalSection(&connCS);
			pInstance = new QueryFactory;
		});
		return pInstance;
	};

	static inline CRITICAL_SECTION connCS;
	friend bool ASSERT_INSUF_BUFFER(QueryFactory* pQF, QueryFactory::BufInfo* pBufInfo, char* end, HRESULT hr);
};

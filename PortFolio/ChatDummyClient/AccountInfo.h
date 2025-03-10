#pragma once
#include <windows.h>
#include <stack>
#include "MemoryPool.h"
struct AccountInfo
{
	INT64 accountNo;
	WCHAR ID[20];
	WCHAR Nick[20];
	char SessionKey[64];

	static constexpr int MaxAccount = 5001;
	static void Init(const WCHAR* pConfigFile);

	__forceinline static const AccountInfo* Alloc()
	{
		return (AccountInfo*)AllocMemoryFromPool(pool_);
	}

private:
	static inline MEMORYPOOL pool_;
};

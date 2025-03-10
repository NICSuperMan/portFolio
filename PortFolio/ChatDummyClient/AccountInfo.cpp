#include "AccountInfo.h"
#include <cstdio>
#include <queue>
#include <random>
#include "Logger.h"

static AccountInfo g_AccountInfo[AccountInfo::MaxAccount];

void AccountInfo::Init(const WCHAR* pConfigFile)
{
	pool_ = CreateMemoryPool(sizeof(AccountInfo), MaxAccount);

	static bool bFirst = true;
	if (!bFirst)
		return;
	bFirst = false;

	FILE* pFile;
	if (0 != _wfopen_s(&pFile, pConfigFile, L"r"))
	{
		LOG(L"ERROR", ERR, TEXTFILE, L"Fail to Read AccountInfo FileName : %s, errno : %d", pConfigFile, errno);
		__debugbreak();
	}

	fseek(pFile, 0, SEEK_END);
	int fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	WCHAR* pArr = new WCHAR[fileSize / 2];
	if (1 != fread_s(pArr, fileSize, fileSize, 1, pFile))
	{
		LOG(L"ERROR", ERR, TEXTFILE, L"Fail to Read AccountInfo FileName : %s, errno : %d", pConfigFile, errno);
		__debugbreak();
	}

	WCHAR* pTemp = pArr + 1;
	WCHAR* cnt = pTemp;

	// 메모리 풀에 있는 모든 AccountInfo에 대해 최초 초기화 이후에는 할당만 받으면됨
	AccountInfo** pReserveForReturn = new AccountInfo*[MaxAccount];
	for (int i = 0; i < AccountInfo::MaxAccount; ++i)
	{
		AccountInfo* pAccountInfo = (AccountInfo*)AllocMemoryFromPool(pool_);
		pTemp = wcstok_s(cnt, L"\t", &cnt);
		pAccountInfo->accountNo = _wtoi(pTemp);
		pTemp = wcstok_s(cnt, L"\t", &cnt);
		wcscpy_s(pAccountInfo->ID, wcslen(pTemp) + 1, pTemp);
		pTemp = wcstok_s(cnt, L"\r\n", &cnt);
		wcscpy_s(pAccountInfo->Nick, wcslen(pTemp) + 1, pTemp);

		//// 세션키 만들기
		memset(pAccountInfo->SessionKey, 1, 64);
		*(INT64*)(pAccountInfo->SessionKey + 56) ^= pAccountInfo->accountNo;
		pReserveForReturn[i] = pAccountInfo;
	}

	for (int i = 0; i < MaxAccount; ++i)
	{
		RetMemoryToPool(pool_, pReserveForReturn[i]);
	}

	delete[] pArr;
	delete[] pReserveForReturn;
}

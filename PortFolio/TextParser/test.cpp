#include <iostream>
#include "Parser.h"

#ifdef _M_IX86
#ifdef _DEBUG
#pragma comment(lib,"TextParserD.lib")
#else
#pragma comment(lib,"TextParser.lib")
#endif
#pragma warning(disable : 4700)
#endif

#ifdef _M_X64
#ifdef _DEBUG
#pragma comment(lib,"TextParserD64.lib")
#else
#pragma comment(lib,"TextParser64.lib")
#endif
#endif

int main()
{
	std::locale::global(std::locale(""));
	PARSER psr = CreateParser(L"Test.txt");
	char* pStart;
	GetValue(psr, L"THREADS", (PVOID*)&pStart, nullptr);
	int value_THREADS = _wtoi((LPCWSTR)pStart);
	
	GetValue(psr, L"KDINNER", (PVOID*)&pStart, nullptr);
	int value_KDINNER = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"LTHREADS", (PVOID*)&pStart, nullptr);
	int value_LTHREADS = _wtoi((LPCWSTR)pStart);

	GetValue(psr, L"MONEY", (PVOID*)&pStart, nullptr);
	float value_MONEY = (float)_wtof((LPCWSTR)pStart);

	char* pEnd;
	WCHAR arr[100];
	GetValue(psr, L"STRING", (PVOID*)&pStart, (PVOID*)&pEnd);
	UINT_PTR length = pEnd - pStart;
	memcpy_s(arr, sizeof(arr), pStart, length);
	arr[length / sizeof(WCHAR)] = 0;
	std::wcout << arr << '\n';

	GetValue(psr, L"STRING2", (PVOID*)&pStart, (PVOID*)&pEnd);
	length = pEnd - pStart;
	memcpy_s(arr, sizeof(arr), pStart, length);
	arr[length / sizeof(WCHAR)] = 0;
	std::wcout << arr << '\n';

	ReleaseParser(psr);
}
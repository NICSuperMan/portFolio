#pragma once
#ifdef PARSERAPI
#else
#define PARSERAPI extern "C" __declspec(dllimport)
#endif 

#include <windows.h>
using PARSER = PVOID;

PARSERAPI PARSER CreateParser(LPCWSTR fileName);
PARSERAPI bool GetValue(PARSER parser, LPCWSTR key, PVOID* pStart, PVOID* pNextOfEnd);
PARSERAPI VOID ReleaseParser(PARSER parser);

// 성공하면 버퍼에 복사하고 0반환 실패하면 복사하지 않고 길이반환 
PARSERAPI int GetValueWSTR(PARSER parser, WCHAR* pOutBuf, int countOfBuf, LPCWSTR key);
PARSERAPI int GetValueINT(PARSER parser, LPCWSTR key);
PARSERAPI unsigned int GetValueUINT(PARSER parser, LPCWSTR key);
PARSERAPI long long GetValueLONGLONG(PARSER parser, LPCWSTR key);
PARSERAPI unsigned long long GetValueULONGLONG(PARSER parser, LPCWSTR key);

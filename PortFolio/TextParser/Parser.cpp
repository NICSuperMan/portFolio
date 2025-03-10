#include "CParser.h"
#define PARSERAPI extern "C" __declspec(dllexport)
#include "Parser.h"

PARSERAPI PARSER CreateParser(LPCWSTR fileName)
{
    CParser* pParser = new CParser;
    pParser->LoadFile(fileName);
    return ((PARSER)(pParser));
}

PARSERAPI bool GetValue(PARSER parser, LPCWSTR key, PVOID* pStart, PVOID* pNextOfEnd)
{
    return ((CParser*)(parser))->GetValue(key, pStart, pNextOfEnd);
}

PARSERAPI VOID ReleaseParser(PARSER parser)
{
    delete ((CParser*)(parser));
}

PARSERAPI int GetValueWSTR(PARSER parser, WCHAR* pOutBuf, int countOfBuf, LPCWSTR key)
{
    char* pStart;
    char* pEnd;
    ((CParser*)(parser))->GetValue(key, (PVOID*)&pStart, (PVOID*)&pEnd);
    int len = (int)(pEnd - pStart) / 2;
    if (countOfBuf >= len)
    {
        wcsncpy_s(pOutBuf, countOfBuf, (const WCHAR*)pStart, len);
        pOutBuf[len] = 0;
        return 0;
    }

    return (int)(pEnd - pStart) / 2;
}

PARSERAPI int GetValueINT(PARSER parser, LPCWSTR key)
{
    char* pStart;
    ((CParser*)(parser))->GetValue(key, (PVOID*)&pStart, nullptr);
    return _wtoi((LPCWSTR)pStart);
}

PARSERAPI unsigned int GetValueUINT(PARSER parser, LPCWSTR key)
{
    char* pStart;
    ((CParser*)(parser))->GetValue(key, (PVOID*)&pStart, nullptr);
    return _wtoi((LPCWSTR)pStart);
}

PARSERAPI long long GetValueLONGLONG(PARSER parser, LPCWSTR key)
{
    char* pStart;
    ((CParser*)(parser))->GetValue(key, (PVOID*)&pStart, nullptr);
    return _wtoi((LPCWSTR)pStart);
}

PARSERAPI unsigned long long GetValueULONGLONG(PARSER parser, LPCWSTR key)
{
    char* pStart;
    ((CParser*)(parser))->GetValue(key, (PVOID*)&pStart, nullptr);
    return _wtoi((LPCWSTR)pStart);
}


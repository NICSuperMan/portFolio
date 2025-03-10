#include <stdio.h>
#include <stdlib.h>
#include <intrin.h>
#include <wchar.h>
#include "CParser.h"


void CParser::LoadFile(const wchar_t* p_file_name)
{
	FILE* pFile;
	_wfopen_s(&pFile, p_file_name, L"rb, ccs = UTF-16LE");
	if (!pFile)	__debugbreak();

	fseek(pFile, 0, SEEK_END);
	long file_size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	pFileBuffer_ = (wchar_t*)malloc(file_size + sizeof(wchar_t));
	if (!pFileBuffer_)	__debugbreak();
	bufferLength_ = (file_size + sizeof(wchar_t)) / sizeof(wchar_t);
	if (fread(pFileBuffer_, file_size, 1, pFile) != 1)	__debugbreak();
	pFileBuffer_[file_size / sizeof(wchar_t)] = '\0'; // 끝을 널문자로 만듬
	fclose(pFile);
}

bool CParser::GetValue(const wchar_t* key, void** pStart, void** pNextOfEnd)
{
	wchar_t* pValueBuffer;
	if (!GetValueHelper(key, &pValueBuffer))
	{
		return false;
	}
	*pStart = pValueBuffer;


	// 문자열이 아닌 경우에는 NULL을 사용자가 넣을것임
	if (pNextOfEnd)
	{
		while (*pValueBuffer != L';')
		{
			++pValueBuffer;
		}
		*pNextOfEnd = pValueBuffer;
	}
	return true;
}

bool CParser::GetValueHelper(const wchar_t* key, wchar_t** ppValueBuffer)
{
	wchar_t* pBuffer = pFileBuffer_;
	++pBuffer; // BOM 건너뜀
	while (FindKey(key, &pBuffer))
	{
		do
		{
			if (!FindKey(L":", &pBuffer))
				goto jump;
			++pBuffer;
			if (*pBuffer == L' ')
				++pBuffer;

			*ppValueBuffer = pBuffer;
			return true;
		} while (0);
	}
jump:
	return false;
}



// 주석 끝문자 즉 \n 혹은 /을 *chppBuffer가 가리키는 상태에서 반환
bool CParser::SkipComment(wchar_t** chppBuffer)
{
	++(*chppBuffer);
	if (**chppBuffer == L'/') // //주석
	{
		while (**chppBuffer != L'\n')
			++(*chppBuffer);
	}
	else if (**chppBuffer == L'*')// /*주석
	{
		++(*chppBuffer);
		while (**chppBuffer != L'*')
			++(*chppBuffer);

		++(*chppBuffer);
		if (**chppBuffer != L'/') // */로 끝나는걸 확인
			__debugbreak();
	}
	else
		return false;

	return true;
}

// 다음 단어의 첫번째 문자를 *ppBuffer가 가리키게함
bool CParser::FindKey(const wchar_t* key, wchar_t** ppBuffer)
{
	wchar_t* pBuffer = *ppBuffer;
	wchar_t* endOfFile = pFileBuffer_ + bufferLength_ - 1;
	int keyLength = (int)wcslen(key);
	int i;
	while (pBuffer < endOfFile)
	{
		// 키의 첫글자가 일치하고 첫글자 바로 전글자가 개행이거나 ; 혹은 공백이면 키 시작조건 만족
		if (*pBuffer == key[0] && (pBuffer[-1] == L'\n' || pBuffer[-1] == L';' || pBuffer[-1] == L' ' || pBuffer[-1] == L'/' || pBuffer[-1] == L'\t'))
		{
			for (i = 1; i < keyLength; ++i)
			{
				if (key[i] != pBuffer[i])
					break;
			}

			// 키의 길이만큼 일치하고 키 끝문자의 다음문자가 공백이면 키 끝조건 만족
			if (i == keyLength && pBuffer[i] == L' ')
			{
				*ppBuffer = pBuffer;
				return true;
			}
			// 일치하지 않을시 처음으로 틀린부분으로 포인터를 이동시킴
			else
			{
				pBuffer += i;
			}
		}
		// 주석 건너뜀
		if (*pBuffer == L'/')
			SkipComment(&pBuffer); // \n 으로 끝나거나 /로 끝나는 포인터를 pBuffer가 가리킴
		++pBuffer;
	}
	return false;
}

CParser::~CParser()
{
	delete[] pFileBuffer_;
	pFileBuffer_ = nullptr;
}
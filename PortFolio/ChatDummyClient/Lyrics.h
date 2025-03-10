#pragma once
#include <windows.h>
struct LyRics
{
	WCHAR pLyrics[MAX_PATH];
	WORD len; // ¹®ÀÚ°¹¼ö

	static constexpr int maxSize = 634;
	static __forceinline const LyRics* Get()
	{
		int idx = (rand() % LyRics::maxSize);
		return &lyricsArr[idx];
	}
	static void Init(const WCHAR* pFileName);
	static LyRics lyricsArr[LyRics::maxSize];
};

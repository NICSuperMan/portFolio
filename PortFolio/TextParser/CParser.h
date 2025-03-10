#pragma once
class CParser
{
public:
	wchar_t* pFileBuffer_;
	int bufferLength_;
	void LoadFile(const wchar_t* p_file_name);
	bool GetValue(const wchar_t* key, void** pStart, void** pNextOfEnd);
	~CParser();
private:
	bool GetValueHelper(const wchar_t* key, wchar_t** ppValueBuffer);
	bool SkipComment(wchar_t** chppBuffer);
	bool FindKey(const wchar_t* key, wchar_t** ppBuffer);
};

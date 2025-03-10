#pragma once
struct LINKED_NODE
{
	const int offset_;
	LINKED_NODE* pNext_;
	LINKED_NODE* pPrev_;
	LINKED_NODE(const int offset);
};

class CLinkedList
{
public:
	int size_ = 0;
	const int offset_;
	LINKED_NODE* pHead_;
	LINKED_NODE* pTail_;
	CLinkedList(const int offset);
	void push_back(void* pNewData);
	void* remove(void* pVictim);
	void* GetNext(void* pCur);
	void* GetFirst();
	int FindElementNum(void* pElement);
	__forceinline size_t size()
	{
		return size_;
	}
};




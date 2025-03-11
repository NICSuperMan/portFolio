#pragma once
#include "CTlsObjectPool.h"
#include "optional"

#include "MultithreadProfiler.h"
template <typename T>
class SpinQ {
private:
    struct Node {
        T data;
        Node* next;
    };

    Node* pHead_;
    Node* pTail_;
    long size_;
    CTlsObjectPool<Node,false> fr;
    BOOL bSpinFlag_;
public:
    SpinQ() : pHead_(nullptr), pTail_(nullptr), size_(0), bSpinFlag_{ FALSE }
    {
        pHead_ = pTail_ = fr.Alloc();
        pHead_->data = -1;
        pHead_->next = nullptr;
        ++size_;
    }

    ~SpinQ() 
    {}

    void enqueue(const T& value) 
    {
        while (InterlockedExchange((LONG*)&bSpinFlag_, TRUE) == TRUE)
        {
            YieldProcessor();
        }

		Node* newNode = fr.Alloc();
		newNode->data = value;
		newNode->next = nullptr;
		pTail_->next = newNode;
		pTail_ = newNode;
		++size_;
		InterlockedExchange((LONG*)&bSpinFlag_, FALSE);
    }

    std::optional<T> dequeue() 
    {
        while (InterlockedExchange((LONG*)&bSpinFlag_, TRUE) == TRUE)
        {
            YieldProcessor();
        }
        if (pHead_ == pTail_)
        {
            InterlockedExchange((LONG*)&bSpinFlag_, FALSE);
            return std::nullopt;
        }

        Node* pDummy;
        Node* pRet;
		pDummy = pHead_;
		pRet = pHead_ = pDummy->next;
        T retData = pRet->data;
		fr.Free(pDummy);
		--size_;

		InterlockedExchange((LONG*)&bSpinFlag_, FALSE);
        return retData;
    }
};

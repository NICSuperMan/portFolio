#pragma once
#include "CTlsObjectPool.h"
#include "optional"

#include "MultithreadProfiler.h"
template <typename T>
class CsQ{
private:
    struct Node {
        T data;
        Node* next;
    };

    Node* pHead_;
    Node* pTail_;
    long size_;
    CTlsObjectPool<Node, false> fr;
    CRITICAL_SECTION cs_;
public:
    CsQ() : pHead_(nullptr), pTail_(nullptr), size_(0)
    {
        InitializeCriticalSection(&cs_);
        pHead_ = pTail_ = fr.Alloc();
        pHead_->data = -1;
        pHead_->next = nullptr;
        ++size_;
    }

    ~CsQ()
    {}

    void enqueue(const T& value)
    {
        EnterCriticalSection(&cs_);
        Node* newNode = fr.Alloc();
        newNode->data = value;
        newNode->next = nullptr;
        pTail_->next = newNode;
        pTail_ = newNode;
        ++size_;
        LeaveCriticalSection(&cs_);
    }

    std::optional<T> dequeue()
    {
        EnterCriticalSection(&cs_);
        if (pHead_ == pTail_)
        {
            LeaveCriticalSection(&cs_);
            return std::nullopt;
        }

        Node* pDummy;
        Node* pRet;
        pDummy = pHead_;
        pRet = pHead_ = pDummy->next;
        T retData = pRet->data;
        fr.Free(pDummy);
        --size_;
        LeaveCriticalSection(&cs_);
        return retData;
    }
};

#pragma once
#include "CTlsObjectPool.h"
#include "optional"

template <typename T>
class CMutexQueue{
private:
    struct Node {
        T data;
        Node* next;
    };

    Node* pHead_;
    Node* pTail_;
    long size_;
    CTlsObjectPool<Node, false> fr;
    HANDLE hMutex_;
    CRITICAL_SECTION cs_;
public:
    CMutexQueue() : pHead_(nullptr), pTail_(nullptr), size_(0)
    {
        hMutex_ = CreateMutex(NULL, FALSE, NULL);
        if (!hMutex_) __debugbreak();
        pHead_ = pTail_ = fr.Alloc();
        pHead_->data = -1;
        pHead_->next = nullptr;
        ++size_;
    }

    ~CMutexQueue()
    {
        CloseHandle(hMutex_);
    }

    void enqueue(const T& value)
    {
        WaitForSingleObject(hMutex_, INFINITE);
        Node* newNode = fr.Alloc();
        newNode->data = value;
        newNode->next = nullptr;
        pTail_->next = newNode;
        pTail_ = newNode;
        ++size_;
        ReleaseMutex(hMutex_);
    }

    std::optional<T> dequeue()
    {
        WaitForSingleObject(hMutex_, INFINITE);
        if (pHead_ == pTail_)
        {
            ReleaseMutex(hMutex_);
            return std::nullopt;
        }

        Node* pDummy;
        Node* pRet;
        pDummy = pHead_;
        pRet = pHead_ = pDummy->next;
        T retData = pRet->data;
        fr.Free(pDummy);
        --size_;
        ReleaseMutex(hMutex_);
        return retData;
    }
};

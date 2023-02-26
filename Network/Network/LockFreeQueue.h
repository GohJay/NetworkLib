#ifndef __LOCK_FREE_QUEUE__H_
#define __LOCK_FREE_QUEUE__H_
#include "LFObjectPool.h"

namespace Jay
{
    /**
    * @file		LockFreeQueue.h
    * @brief	Lock-Free Queue Template Class
    * @details	CAS 연산으로 구현한 구현한 락프리 큐 클래스
    * @author	고재현
    * @date		2023-01-08
    * @version	1.0.0
    **/
    template<typename T>
    class LockFreeQueue
    {
    private:
        struct NODE
        {
            T data;
            NODE* next;
        };
    public:
        LockFreeQueue() : _count(0), _nodePool(0, false)
        {
            _head = _nodePool.Alloc();
            _head->next = nullptr;
            _tail = _head;
        }
        ~LockFreeQueue()
        {
            T temp;
            while (Dequeue(temp))
            {
            }

            _head = (NODE*)GET_NODE_ADDRESS(_head);
            _nodePool.Free(_head);
        }
    public:
        void Enqueue(T data)
        {
            NODE* tailNode;
            NODE* tail;
            NODE* tailNext;
            NODE* node;
            NODE* nextNode;
            LONG64 nodeStamp;

            node = _nodePool.Alloc();
            node->data = data;
            node->next = nullptr;

            for (;;)
            {
                tailNode = _tail;
                tail = (NODE*)GET_NODE_ADDRESS(tailNode);
                tailNext = tail->next;

                if (tailNext == nullptr)
                {
                    if (InterlockedCompareExchangePointer((PVOID*)&tail->next, node, tailNext) == tailNext)
                    {
                        nodeStamp = NEXT_NODE_STAMP(tailNode);
                        nextNode = (NODE*)MAKE_NODE(node, nodeStamp);
                        InterlockedCompareExchangePointer((PVOID*)&_tail, nextNode, tailNode);
                        break;
                    }
                }
                else
                {
                    nodeStamp = NEXT_NODE_STAMP(tailNode);
                    nextNode = (NODE*)MAKE_NODE(tailNext, nodeStamp);
                    InterlockedCompareExchangePointer((PVOID*)&_tail, nextNode, tailNode);
                }
            }

            InterlockedIncrement(&_count);
        }
        bool Dequeue(T& data)
        {
            NODE* headNode;
            NODE* head;
            NODE* headNext;
            NODE* nextNode;
            LONG64 nodeStamp;

            if (InterlockedDecrement(&_count) < 0)
            {
                InterlockedIncrement(&_count);
                return false;
            }

            for (;;)
            {
                headNode = _head;
                head = (NODE*)GET_NODE_ADDRESS(headNode);
                headNext = head->next;

                if (headNext == nullptr)
                    continue;

                nodeStamp = NEXT_NODE_STAMP(headNode);
                nextNode = (NODE*)MAKE_NODE(headNext, nodeStamp);

                if (headNode == _tail)
                    InterlockedCompareExchangePointer((PVOID*)&_tail, nextNode, headNode);

                data = headNext->data;
                if (InterlockedCompareExchangePointer((PVOID*)&_head, nextNode, headNode) == headNode)
                    break;
            }

            _nodePool.Free(head);
            return true;
        }
        inline int size()
        {
            return _count;
        }
        inline int empty()
        {
            return _count <= 0;
        }
    private:
        NODE* _head;
        NODE* _tail;
        long _count;
        LFObjectPool<NODE> _nodePool;
    };
}

#endif

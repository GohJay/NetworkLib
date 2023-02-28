#ifndef __LOCK_FREE_STACK__H_
#define __LOCK_FREE_STACK__H_
#include "LFObjectPool.h"

namespace Jay
{
    /**
    * @file		LFStack.h
    * @brief	Lock-Free Stack Template Class
    * @details	CAS 연산으로 구현한 구현한 락프리 스택 클래스
    * @author	고재현
    * @date		2023-01-08
    * @version	1.0.0
    **/
    template<typename T>
    class LFStack
    {
    private:
        struct NODE
        {
            T data;
            NODE* prev;
        };
    public:
        LFStack() : _top(nullptr), _nodePool(0, false)
        {
        }
        ~LFStack()
        {
            T temp;
            while (Pop(temp))
            {
            }
        }
    public:
        void Push(T data)
        {
            NODE* top;
            NODE* prev;
            NODE* node;
            LONG64 nodeStamp;

            node = _nodePool.Alloc();
            node->data = data;

            do
            {
                top = _top;
                node->prev = (NODE*)GET_NODE_ADDRESS(top);
                nodeStamp = NEXT_NODE_STAMP(top);
                prev = (NODE*)MAKE_NODE(node, nodeStamp);
            } while (InterlockedCompareExchangePointer((PVOID*)&_top, prev, top) != top);

            InterlockedIncrement(&_count);
        }
        bool Pop(T& data)
        {
            NODE* top;
            NODE* prev;
            NODE* node;
            LONG64 nodeStamp;

            if (InterlockedDecrement(&_count) < 0)
            {
                InterlockedIncrement(&_count);
                return false;
            }

            do
            {
                top = _top;
                node = (NODE*)GET_NODE_ADDRESS(top);
                nodeStamp = GET_NODE_STAMP(top);
                prev = (NODE*)MAKE_NODE(node->prev, nodeStamp);
            } while (InterlockedCompareExchangePointer((PVOID*)&_top, prev, top) != top);
			
            data = node->data;
            _nodePool.Free(node);

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
        NODE* _top;
        long _count;
        LFObjectPool<NODE> _nodePool;
    };
}

#endif

#ifndef  __LFOBJECT_POOL__H_
#define  __LFOBJECT_POOL__H_
#ifdef _WIN64

#define SECURE_MODE					1

#define MAXIMUM_ADDRESS_RANGE		0x00007ffffffeffff
#define MAXIMUM_ADDRESS_MASK		0x00007fffffffffff
#define NODE_STAMP_MASK				0xffff800000000000
#define NODE_STAMP_OFFSET			47

#define GET_NODE_STAMP(node)		(LONG64)((ULONG_PTR)(node) & NODE_STAMP_MASK)
#define GET_NODE_ADDRESS(node)		(PVOID)((ULONG_PTR)(node) & MAXIMUM_ADDRESS_MASK)
#define NEXT_NODE_STAMP(basenode)	(LONG64)((((LONG64)(basenode) >> NODE_STAMP_OFFSET) + 1) << NODE_STAMP_OFFSET)
#define MAKE_NODE(address, stamp)	(PVOID)((ULONG_PTR)(address) | (stamp))

#include <new.h>

namespace Jay
{
	/**
	* @file		LFObjectPool.h
	* @brief	오브젝트 메모리 풀 클래스(오브젝트 풀 / 프리리스트)
	* @details	특정 데이터를(구조체, 클래스, 변수) 일정량 할당 후 나눠쓴다.
	* @usage	Jay::LFObjectPool<T> MemPool(300, false);
				T *pData = MemPool.Alloc();
				pData 사용
				MemPool.Free(pData);
	* @author	고재현
	* @date		2022-12-08
	* @version	1.1.3
	**/
	template <typename T>
	class LFObjectPool
	{
	private:
		struct NODE
		{
			T data;
#if SECURE_MODE
			size_t signature;
#endif
			NODE* prev;
		};
	public:
		/**
		* @brief	생성자, 소멸자
		* @details
		* @param	int(초기 블럭 개수), bool(Alloc 시 생성자 / Free 시 파괴자 호출 여부)
		* @return	
		**/
		LFObjectPool(int blockNum, bool placementNew = false) 
			: _top(nullptr), _placementNew(placementNew), _capacity(0), _useCount(0)
		{
			NODE* node; 
			LONG64 nodeStamp;
			while (blockNum > 0)
			{
				node = (NODE*)malloc(sizeof(NODE));
				node->prev = (NODE*)GET_NODE_ADDRESS(_top);
#if SECURE_MODE
				node->signature = (size_t)this;
				_capacity++;
#endif
				if (!_placementNew)
					new(&node->data) T();

				nodeStamp = NEXT_NODE_STAMP(_top);
				_top = (NODE*)MAKE_NODE(node, nodeStamp);

				blockNum--;
			}
		}
		~LFObjectPool()
		{
			NODE* prev;
			_top = (NODE*)GET_NODE_ADDRESS(_top);
			while (_top)
			{
				if (!_placementNew)
					_top->data.~T();

				prev = _top->prev;
				free(_top);
				_top = prev;
#if SECURE_MODE
				_capacity--;
#endif
			}
		}
	public:
		/**
		* @brief	블럭 하나를 할당받는다.
		* @details
		* @param	void
		* @return	T*(데이터 블럭 포인터)
		**/
		T* Alloc(void)
		{
			NODE* top;
			NODE* prev;
			NODE* node;
			LONG64 nodeStamp;

			do
			{
				top = _top;
				node = (NODE*)GET_NODE_ADDRESS(top);
				if (node == nullptr)
				{
					node = (NODE*)malloc(sizeof(NODE));
					new(&node->data) T();
#if SECURE_MODE
					node->signature = (size_t)this;
					InterlockedIncrement(&_capacity);
					InterlockedIncrement(&_useCount);
#endif
					return &node->data;
				}
				nodeStamp = GET_NODE_STAMP(top);
				prev = (NODE*)MAKE_NODE(node->prev, nodeStamp);
			} while (InterlockedCompareExchangePointer((PVOID*)&_top, prev, top) != top);

			if (_placementNew)
				new(&node->data) T();
#if SECURE_MODE
			InterlockedIncrement(&_useCount);
#endif
			return &node->data;
		}
		
		/**
		* @brief	사용중이던 블럭을 해제한다.
		* @details	
		* @param	T*(데이터 블럭 포인터)
		* @return	void
		**/
		void Free(T* data) throw(...)
		{
			NODE* node;
			NODE* top;
			NODE* prev;
			LONG64 nodeStamp;

			node = (NODE*)data;
#if SECURE_MODE
			if (node->signature != (size_t)this)
				throw;
#endif
			if (_placementNew)
				node->data.~T();

			do
			{
				top = _top;
				node->prev = (NODE*)GET_NODE_ADDRESS(top);
				nodeStamp = NEXT_NODE_STAMP(top);
				prev = (NODE*)MAKE_NODE(node, nodeStamp);
			} while (InterlockedCompareExchangePointer((PVOID*)&_top, prev, top) != top);

#if SECURE_MODE
			InterlockedDecrement(&_useCount);
#endif
		}
		
		/**
		* @brief	현재 확보 된 블럭 개수를 얻는다.(메모리 풀 내부의 전체 개수)
		* @details
		* @param	void
		* @return	int(메모리 풀 내부 전체 블럭 개수)
		**/
		inline int GetCapacityCount(void)
		{
			return _capacity;
		}

		/**
		* @brief	현재 사용중인 블럭 개수를 얻는다.
		* @details
		* @param	void
		* @return	int(사용중인 블럭 개수)
		**/
		inline int GetUseCount(void)
		{
			return _useCount;
		}
	private:
		NODE* _top;
		bool _placementNew;
		long _capacity;
		long _useCount;
	};
}

#endif //!_WIN64
#endif //!__OBJECT_POOL__H_

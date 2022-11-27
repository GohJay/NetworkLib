#ifndef  __OBJECT_POOL__H_
#define  __OBJECT_POOL__H_

#define SECURE_MODE 1
#ifdef _WIN64
#define MEM_GUARD 0xfdfdfdfdfdfdfdfd
#else
#define MEM_GUARD 0xfdfdfdfd
#endif

#include <new.h>
#include <exception>
#include <synchapi.h>

namespace Jay
{
	/**
	* @file		ObjectPool.h
	* @brief	오브젝트 메모리 풀 클래스(오브젝트 풀 / 프리리스트)
	* @details	특정 데이터를(구조체, 클래스, 변수) 일정량 할당 후 나눠쓴다.
	* @usage	Jay::ObjectPool<T> MemPool(300, false);
				T *pData = MemPool.Alloc();
				pData 사용
				MemPool.Free(pData);
	* @author   고재현
	* @date		2022-11-26
	* @version  1.0.3
	**/
	template <typename T>
	class ObjectPool
	{
	private:
		struct NODE
		{
#if SECURE_MODE
			size_t signature;
			size_t underflowGuard;
			T data;
			size_t overflowGuard;
			NODE* prev;
#else
			T data;
			NODE* prev;
#endif
		};
	public:
		/**
		* @brief	생성자, 소멸자
		* @details
		* @param	int(초기 블럭 개수), bool(Alloc 시 생성자 / Free 시 파괴자 호출 여부)
		* @return	
		**/
		ObjectPool(int blockNum, bool placementNew = false) 
			: _top(nullptr), _placementNew(placementNew), _capacity(blockNum), _useCount(0)
		{
			InitializeSRWLock(&_poolLock);
			while (blockNum > 0)
			{
				NODE* block = (NODE*)malloc(sizeof(NODE));
#if SECURE_MODE
				block->signature = (size_t)this;
				block->underflowGuard = MEM_GUARD;
				block->overflowGuard = MEM_GUARD;
#endif
				block->prev = _top;
				if (!_placementNew)
					new(&block->data) T();
				_top = block;
				blockNum--;
			}
		}
		~ObjectPool()
		{
			while (_top)
			{
				NODE* prev = _top->prev;
				if (!_placementNew)
					_top->data.~T();
				free(_top);
				_top = prev;
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
			AcquireSRWLockExclusive(&_poolLock);
			NODE* block;
			if (_top == nullptr)
			{
				block = (NODE*)malloc(sizeof(NODE));
#if SECURE_MODE
				block->signature = (size_t)this;
				block->underflowGuard = MEM_GUARD;
				block->overflowGuard = MEM_GUARD;
#endif
				block->prev = _top;
				new(&block->data) T();
			}
			else
			{
				block = _top;
				if (_placementNew)
					new(&block->data) T();
				_top = _top->prev;
				_capacity--;
			}
			_useCount++;
			ReleaseSRWLockExclusive(&_poolLock);
			return &block->data;
		}
		
		/**
		* @brief	사용중이던 블럭을 해제한다.
		* @details	
		* @param	T*(데이터 블럭 포인터)
		* @return	void
		**/
#if SECURE_MODE
		void Free(T* data) throw(...)
		{
			AcquireSRWLockExclusive(&_poolLock);
			NODE* block = (NODE*)((char*)data - sizeof(size_t) - sizeof(size_t));
			if (block->signature != (size_t)this)
				throw std::exception("Incorrect signature");
			if (block->underflowGuard != MEM_GUARD)
				throw std::exception("Memory underflow");
			if (block->overflowGuard != MEM_GUARD)
				throw std::exception("Memory overflow");
#else
		void Free(T* data)
		{
			AcquireSRWLockExclusive(&_poolLock);
			NODE* block = (NODE*)data;
#endif
			if (_placementNew)
				block->data.~T();
			block->prev = _top;
			_top = block;
			_useCount--;
			_capacity++;
			ReleaseSRWLockExclusive(&_poolLock);
		}
		
		/**
		* @brief	현재 확보 된 블럭 개수를 얻는다.(메모리 풀 내부의 전체 개수)
		* @details
		* @param	void
		* @return	int(메모리 풀 내부 전체 블럭 개수)
		**/
		int GetCapacityCount(void) { return _capacity; }

		/**
		* @brief	현재 사용중인 블럭 개수를 얻는다.
		* @details
		* @param	void
		* @return	int(사용중인 블럭 개수)
		**/
		int GetUseCount(void) { return _useCount; }
	private:
		NODE* _top;
		bool _placementNew;
		int _capacity;
		int _useCount;
		SRWLOCK _poolLock;
	};
}
#endif

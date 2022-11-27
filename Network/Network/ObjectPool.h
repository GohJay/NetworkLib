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
	* @brief	������Ʈ �޸� Ǯ Ŭ����(������Ʈ Ǯ / ��������Ʈ)
	* @details	Ư�� �����͸�(����ü, Ŭ����, ����) ������ �Ҵ� �� ��������.
	* @usage	Jay::ObjectPool<T> MemPool(300, false);
				T *pData = MemPool.Alloc();
				pData ���
				MemPool.Free(pData);
	* @author   ������
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
		* @brief	������, �Ҹ���
		* @details
		* @param	int(�ʱ� �� ����), bool(Alloc �� ������ / Free �� �ı��� ȣ�� ����)
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
		* @brief	�� �ϳ��� �Ҵ�޴´�.
		* @details
		* @param	void
		* @return	T*(������ �� ������)
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
		* @brief	������̴� ���� �����Ѵ�.
		* @details	
		* @param	T*(������ �� ������)
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
		* @brief	���� Ȯ�� �� �� ������ ��´�.(�޸� Ǯ ������ ��ü ����)
		* @details
		* @param	void
		* @return	int(�޸� Ǯ ���� ��ü �� ����)
		**/
		int GetCapacityCount(void) { return _capacity; }

		/**
		* @brief	���� ������� �� ������ ��´�.
		* @details
		* @param	void
		* @return	int(������� �� ����)
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

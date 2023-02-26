#ifndef  __LFOBJECT_POOL_TLS__H_
#define  __LFOBJECT_POOL_TLS__H_
#include "LFObjectPool.h"

#define CHUNK_UNIT		300

namespace Jay
{
	/**
	* @file		LFObjectPool_TLS.h
	* @brief	오브젝트 메모리 풀 클래스(오브젝트 풀 / 프리리스트) TLS 버전
	* @details	특정 데이터를(구조체, 클래스, 변수) 일정량 스레드별로 할당 후 나눠쓴다.
	* @usage	Jay::LFObjectPool_TLS<T> MemPool(300, false);
				T *pData = MemPool.Alloc();
				pData 사용
				MemPool.Free(pData);
	* @author	고재현
	* @date		2022-12-31
	* @version	1.0.0
	**/
	template <typename T>
	class LFObjectPool_TLS
	{
	private:
		struct NODE
		{
			NODE() {}
			~NODE() {}
			T data;
#if SECURE_MODE
			size_t signature;
#endif
			PVOID chunk;
		};
		struct CHUNK
		{
			NODE nodeArray[CHUNK_UNIT];
			LONG allocCount;
			LONG freeCount;
		};
	public:
		/**
		* @brief	생성자, 소멸자
		* @details
		* @param	int(초기 청크 개수), bool(Alloc 시 생성자 / Free 시 파괴자 호출 여부)
		* @return	
		**/
		LFObjectPool_TLS(int chunkNum, bool placementNew = false)
			: _chunkPool(chunkNum, placementNew)
		{
			_tlsChunk = TlsAlloc();
			if (_tlsChunk == TLS_OUT_OF_INDEXES)
				throw;
		}
		~LFObjectPool_TLS()
		{
			TlsFree(_tlsChunk);
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
			NODE* node;
			CHUNK* chunk;
			
			chunk = (CHUNK*)TlsGetValue(_tlsChunk);
			if (chunk == NULL)
			{
				chunk = _chunkPool.Alloc();
				chunk->allocCount = 0;
				chunk->freeCount = 0;
				TlsSetValue(_tlsChunk, chunk);
			}

			node = &chunk->nodeArray[chunk->allocCount];
			node->chunk = chunk;
#if SECURE_MODE
			node->signature = (size_t)this;
#endif
			chunk->allocCount++;
			if (chunk->allocCount == CHUNK_UNIT)
				TlsSetValue(_tlsChunk, NULL);

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
			CHUNK* chunk;
			LONG count;

			node = (NODE*)data;
#if SECURE_MODE
			if (node->signature != (size_t)this)
				throw;
#endif
			chunk = (CHUNK*)node->chunk;
			count = InterlockedIncrement(&chunk->freeCount);
			if (count == CHUNK_UNIT)
				_chunkPool.Free(chunk);
		}

		/**
		* @brief	현재 확보 된 블럭 개수를 얻는다.(메모리 풀 내부의 전체 개수)
		* @details
		* @param	void
		* @return	int(메모리 풀 내부 전체 블럭 개수)
		**/
		inline int GetCapacityCount(void)
		{
			return _chunkPool.GetCapacityCount() * CHUNK_UNIT;
		}

		/**
		* @brief	현재 사용중인 블럭 개수를 얻는다.
		* @details
		* @param	void
		* @return	int(사용중인 블럭 개수)
		**/
		inline int GetUseCount(void)
		{
			return _chunkPool.GetUseCount() * CHUNK_UNIT;
		}
	private:
		DWORD _tlsChunk;
		LFObjectPool<CHUNK> _chunkPool;
	};
}

#endif //!__OBJECT_POOL_TLS__H_

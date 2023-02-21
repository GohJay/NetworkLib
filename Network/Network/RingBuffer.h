#ifndef __RINGBUFFER__H_
#define __RINGBUFFER__H_
#include "Base.h"

namespace Jay
{
	class LanClient;
	class LanServer;
	class NetClient;
	class NetServer;
	class NetServerEx;
	class RingBuffer
	{
		/**
		* @file		RingBuffer.h
		* @brief	Network RingBuffer Class
		* @details	TCP/IP 프로토콜 송수신을 위한 링버퍼 클래스
		* @author	고재현
		* @date		2022-11-26
		* @version	1.0.5
		**/
	public:
		RingBuffer(int bufferSize = 8192);
		~RingBuffer();
	public:
		/**
		* @brief	현재 버퍼에 남은 용량 얻기
		* @details
		* @param	void
		* @return	int(남은 용량)
		**/
		int GetFreeSize(void);

		/**
		* @brief	현재 사용중인 용량 얻기
		* @details
		* @param	void
		* @return	int(사용중인 용량)
		**/
		int GetUseSize(void);

		/**
		* @brief	버퍼 포인터로 외부에서 한방에 읽고 쓸 수 있는 길이
		* @details	원형 큐의 구조상 버퍼의 끝단에 있는 데이터는 끝 -> 처음으로 돌아가서 2번에 데이터를 얻거나 넣을 수 있음.
		*			이 부분에서 끊어지지 않은 길이를 의미.
		* @param	void
		* @return	int(사용가능 용량)
		**/
		int	DirectEnqueueSize(void);
		int	DirectDequeueSize(void);

		/**
		* @brief	버퍼의 전체 용량 얻기
		* @details
		* @param	void
		* @return	int(버퍼 전체 용량)
		**/
		int GetBufferSize(void);

		/**
		* @brief	쓰기위치에 데이터 넣기
		* @details
		* @param	const char*(데이터 포인터), int(크기)
		* @return	int(넣은 크기)
		**/
		int Enqueue(const char* input, int size);

		/**
		* @brief	읽기위치에서 데이터 가져오기
		* @details
		* @param	char*(데이터 포인터), int(크기)
		* @return	int(가져온 크기)
		**/
		int Dequeue(char* output, int size);

		/**
		* @brief	원하는 길이만큼 읽기위치에서 삭제 / 쓰기위치 이동
		* @details
		* @param	char*(데이터 포인터), int(크기)
		* @return	int(가져온 크기)
		**/
		int Peek(char* output, int size);

		/**
		* @brief	버퍼의 모든 데이터 삭제
		* @details
		* @param	void
		* @return	void
		**/
		void ClearBuffer(void);
	private:
		/**
		* @brief	원하는 길이만큼 읽기위치에서 삭제 / 쓰기위치 이동
		* @details
		* @param	int(원하는 길이)
		* @return	void
		**/
		void MoveFront(int size);
		void MoveRear(int size);

		/**
		* @brief	버퍼의 Front 포인터 얻음
		* @details
		* @param	void
		* @return	char*(버퍼 포인터)
		**/
		char* GetFrontBufferPtr(void);

		/**
		* @brief	버퍼의 Rear 포인터 얻음
		* @details
		* @param	void
		* @return	char*(버퍼 포인터)
		**/
		char* GetRearBufferPtr(void);

		/**
		* @brief	버퍼의 시작 포인터 얻음
		* @details
		* @param	void
		* @return	char*(버퍼 포인터)
		**/
		char* GetBufferPtr(void);
	private:
		char* _buffer;
		char* _bufferEnd;
		char* _front;
		char* _rear;
		int _bufferSize;
		friend class NetServerEx;
		friend class NetServer;
		friend class NetClient;
		friend class LanServer;
		friend class LanClient;
	};
}

#endif

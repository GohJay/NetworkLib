#ifndef __RINGBUFFER__H_
#define __RINGBUFFER__H_
#include "../../Common/Base.h"

JAYNAMESPACE
/**
* @file		RingBuffer.h
* @brief	Network RingBuffer Class
* @details	TCP/IP 프로토콜 송수신을 위한 링버퍼 클래스
* @author   고재현
* @date		2022-08-04
* @version  1.0.0
**/
class RingBuffer
{
public:
	RingBuffer(int bufferSize = 1000);
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
	* @brief	writePos 에 데이터 넣기
	* @details
	* @param	const char*(데이터 포인터), int(크기)
	* @return	bool(가능 여부)
	**/
	bool Enqueue(const char *input, int size);

	/**
	* @brief	readPos 에서 데이터 가져오기
	* @details
	* @param	char*(데이터 포인터), int(크기)
	* @return	bool(가능 여부)
	**/
	bool Dequeue(char *output, int size);

	/**
	* @brief	원하는 길이만큼 readPos 에서 삭제 / writePos 이동
	* @details
	* @param	char*(데이터 포인터), int(크기)
	* @return	bool(가능 여부)
	**/
	bool Peek(char *output, int size);

	/**
	* @brief	버퍼의 모든 데이터 삭제
	* @details
	* @param	void
	* @return	void
	**/
	void ClearBuffer(void);
private:
	char* _buffer;
	char* _bufferEnd;
	int _bufferSize;
	char* _front;
	char* _rear;
};
JAYNAMESPACEEND

#endif

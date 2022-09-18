#ifndef __SERIALIZATIONBUFFER__H_
#define __SERIALIZATIONBUFFER__H_
#include "../../Common/Base.h"

JAYNAMESPACE
/**
* @file		SerializationBuffer.h
* @brief	Network SerializationBuffer Class
* @details	네트워크 송수신을 위한 직렬화버퍼 클래스
* @author   고재현
* @date		2022-08-20
* @version  1.0.2
**/
class SerializationBuffer
{
public:
	SerializationBuffer(int bufferSize = 256);
	virtual ~SerializationBuffer();
public:
	/**
	* @brief	버퍼 사이즈 얻기
	* @details
	* @param	void
	* @return	int(버퍼 사이즈)
	**/
	int	GetBufferSize(void);

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
	* @brief	버퍼 크기 조정
	* @details
	* @param	int(조정할 버퍼 크기)
	* @return	void
	**/
	void Resize(int bufferSize);

	/**
	* @brief	원하는 길이만큼 읽기위치에서 삭제 / 쓰기위치 이동
	* @details
	* @param	int(원하는 길이)
	* @return	void
	**/
	void MoveFront(int size);
	void MoveRear(int size);

	/**
	* @brief	버퍼의 모든 데이터 삭제
	* @details
	* @param	void
	* @return	void
	**/
	void ClearBuffer(void);

	/**
	* @brief	버퍼 포인터 얻기
	* @details
	* @param	void
	* @return	char*(버퍼 포인터)
	**/
	char *GetBufferPtr(void);

	/**
	* @brief	데이터 입력
	* @details
	* @param	const char*(데이터 포인터), int(크기)
	* @return	int(입력된 데이터 크기)
	**/
	int	PutData(const char *input, int size);

	/**
	* @brief	데이터 출력
	* @details
	* @param	char*(데이터 포인터), int(크기)
	* @return	int(출력된 데이터 크기)
	**/
	int	GetData(char *output, int size);
public:
	SerializationBuffer	&operator = (const SerializationBuffer &packet);

	SerializationBuffer	&operator << (const char value);
	SerializationBuffer	&operator << (const unsigned char value);

	SerializationBuffer	&operator << (const wchar_t value);

	SerializationBuffer	&operator << (const short value);
	SerializationBuffer	&operator << (const unsigned short value);

	SerializationBuffer	&operator << (const long value);
	SerializationBuffer	&operator << (const unsigned long value);

	SerializationBuffer	&operator << (const long long value);
	SerializationBuffer	&operator << (const unsigned long long value);

	SerializationBuffer	&operator << (const int value);
	SerializationBuffer	&operator << (const unsigned int value);

	SerializationBuffer	&operator << (const float value);
	SerializationBuffer	&operator << (const double value);

	SerializationBuffer	&operator >> (char &value);
	SerializationBuffer	&operator >> (unsigned char &value);

	SerializationBuffer	&operator >> (const wchar_t value);

	SerializationBuffer	&operator >> (short &value);
	SerializationBuffer	&operator >> (unsigned short &value);

	SerializationBuffer	&operator >> (long &value);
	SerializationBuffer	&operator >> (unsigned long &value);

	SerializationBuffer	&operator >> (long long &value);
	SerializationBuffer	&operator >> (unsigned long long &value);

	SerializationBuffer	&operator >> (int &value);
	SerializationBuffer	&operator >> (unsigned int &value);

	SerializationBuffer	&operator >> (float &value);
	SerializationBuffer	&operator >> (double &value);
protected:
	char* _buffer;
	char* _bufferEnd;
	char* _front;
	char* _rear;
	int _bufferSize;
};
JAYNAMESPACEEND

#endif

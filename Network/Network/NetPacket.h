#ifndef __NETPACKET__H_
#define __NETPACKET__H_
#include "Base.h"
#include "ObjectPool_TLS.h"

namespace Jay
{
	class LanClient;
	class LanServer;
	class NetClient;
	class NetServer;
	class NetPacketPtr;
	class NetPacket
	{
		/**
		* @file		NetPacket.h
		* @brief	Network Packet Class (SerializationBuffer)
		* @details	네트워크 송수신을 위한 직렬화버퍼 클래스
		* @author   고재현
		* @date		2023-01-22
		* @version  1.0.8
		**/
	private:
		NetPacket(int bufferSize = 1024);
		~NetPacket();
	public:
		/**
		* @brief	직렬화 버퍼 할당
		* @details
		* @param	void
		* @return	NetPacket*(직렬화 버퍼 포인터)
		**/
		static NetPacket* Alloc(void);

		/**
		* @brief	직렬화 버퍼 해제
		* @details
		* @param	NetPacket*(직렬화 버퍼 포인터)
		* @return	void
		**/
		static void Free(NetPacket* packet);

		/**
		* @brief	버퍼의 참조 카운트 증가
		* @details
		* @param	void
		* @return	int(증가된 참조 카운트 값)
		**/
		long IncrementRefCount(void);

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
		* @brief	버퍼의 참조 카운트 얻기
		* @details
		* @param	void
		* @return	int(참조 카운트)
		**/
		int	GetRefCount(void);

		/**
		* @brief	버퍼 크기 조정
		* @details
		* @param	int(조정할 버퍼 크기)
		* @return	void
		**/
		void Resize(int bufferSize);

		/**
		* @brief	버퍼의 모든 데이터 삭제
		* @details
		* @param	void
		* @return	void
		**/
		void ClearBuffer(void);

		/**
		* @brief	데이터 입력
		* @details
		* @param	const char*(데이터 포인터), int(크기)
		* @return	int(입력된 데이터 크기)
		**/
		int	PutData(const char* input, int size);

		/**
		* @brief	데이터 출력
		* @details
		* @param	char*(데이터 포인터), int(크기)
		* @return	int(출력된 데이터 크기)
		**/
		int	GetData(char* output, int size);
	private:
		/**
		* @brief	패킷 사이즈 얻기
		* @details
		* @param	void
		* @return	int(패킷 사이즈)
		**/
		int GetPacketSize(void);

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
		* @brief	버퍼의 Header 포인터 얻음
		* @details
		* @param	void
		* @return	char*(버퍼 포인터)
		**/
		char* GetHeaderPtr(void);

		/**
		* @brief	헤더 입력
		* @details
		* @param	const char*(헤더 포인터), int(크기)
		* @return	int(입력된 헤더 크기)
		**/
		int PutHeader(const char* header, int size);

		/**
		* @brief	패킷 인코딩 처리
		* @details
		* @param	BYTE(패킷 코드), BYTE(패킷 대칭키)
		* @return	void
		**/
		void Encode(BYTE code, BYTE key);

		/**
		* @brief	패킷 디코딩 처리
		* @details
		* @param	BYTE(패킷 코드), BYTE(패킷 대칭키)
		* @return	bool(패킷 디코딩 결과)
		**/
		bool Decode(BYTE code, BYTE key);

		/**
		* @brief	패킷 암호화 처리
		* @details
		* @param	BYTE(패킷 대칭키)
		* @return	void
		**/
		void Encrypt(BYTE key);

		/**
		* @brief	패킷 복호화 처리
		* @details
		* @param	BYTE(패킷 대칭키)
		* @return	void
		**/
		void Decrypt(BYTE key);

		/**
		* @brief	패킷 체크섬 얻기
		* @details
		* @param	void
		* @return	unsigned char(패킷 체크섬)
		**/
		unsigned char MakeChecksum(void);
	public:
		NetPacket& operator = (const NetPacket& packet);

		NetPacket& operator << (const char value) throw(...);
		NetPacket& operator << (const unsigned char value) throw(...);

		NetPacket& operator << (const short value) throw(...);
		NetPacket& operator << (const unsigned short value) throw(...);

		NetPacket& operator << (const long value) throw(...);
		NetPacket& operator << (const unsigned long value) throw(...);

		NetPacket& operator << (const long long value) throw(...);
		NetPacket& operator << (const unsigned long long value) throw(...);

		NetPacket& operator << (const int value) throw(...);
		NetPacket& operator << (const unsigned int value) throw(...);

		NetPacket& operator << (const float value) throw(...);
		NetPacket& operator << (const double value) throw(...);

		NetPacket& operator >> (char& value) throw(...);
		NetPacket& operator >> (unsigned char& value) throw(...);

		NetPacket& operator >> (short& value) throw(...);
		NetPacket& operator >> (unsigned short& value) throw(...);

		NetPacket& operator >> (long& value) throw(...);
		NetPacket& operator >> (unsigned long& value) throw(...);

		NetPacket& operator >> (long long& value) throw(...);
		NetPacket& operator >> (unsigned long long& value) throw(...);

		NetPacket& operator >> (int& value) throw(...);
		NetPacket& operator >> (unsigned int& value) throw(...);

		NetPacket& operator >> (float& value) throw(...);
		NetPacket& operator >> (double& value) throw(...);
	private:
		char* _buffer;
		char* _bufferEnd;
		char* _front;
		char* _rear;
		char* _header;
		int _bufferSize;
		long _refCount;
		bool _encode;
		static ObjectPool_TLS<NetPacket> _packetPool;
		friend class ObjectPool_TLS<NetPacket>;
		friend class NetPacketPtr;
		friend class NetServer;
		friend class NetClient;
		friend class LanServer;
		friend class LanClient;
	};
}

#endif

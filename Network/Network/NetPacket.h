#ifndef __NETPACKET__H_
#define __NETPACKET__H_
#include "Base.h"
#include "ObjectPool.h"

JAYNAMESPACE
class LanServer;
class NetPacketPtr;
class NetPacket
{
	/**
	* @file		NetPacket.h
	* @brief	Network Packet Class (SerializationBuffer)
	* @details	��Ʈ��ũ �ۼ����� ���� ����ȭ���� Ŭ����
	* @author   ������
	* @date		2022-12-03
	* @version  1.0.6
	**/
protected:
	NetPacket(int bufferSize = 1024);
	virtual ~NetPacket();

	/**
	* @brief	����ȭ ���� �Ҵ�
	* @details
	* @param	void
	* @return	NetPacket*(����ȭ ���� ������)
	**/
	static NetPacket* Alloc(void);

	/**
	* @brief	����ȭ ���� ����
	* @details
	* @param	NetPacket*(����ȭ ���� ������)
	* @return	void
	**/
	static void Free(NetPacket* packet);
public:
	/**
	* @brief	���� ������ ���
	* @details
	* @param	void
	* @return	int(���� ������)
	**/
	int	GetBufferSize(void);

	/**
	* @brief	���� ���ۿ� ���� �뷮 ���
	* @details
	* @param	void
	* @return	int(���� �뷮)
	**/
	int GetFreeSize(void);

	/**
	* @brief	���� ������� �뷮 ���
	* @details
	* @param	void
	* @return	int(������� �뷮)
	**/
	int GetUseSize(void);

	/**
	* @brief	������ ���� ī��Ʈ ���
	* @details
	* @param	void
	* @return	int(���� ī��Ʈ)
	**/
	int	GetRefCount(void);

	/**
	* @brief	���� ũ�� ����
	* @details
	* @param	int(������ ���� ũ��)
	* @return	void
	**/
	void Resize(int bufferSize);

	/**
	* @brief	������ ��� ������ ����
	* @details
	* @param	void
	* @return	void
	**/
	void ClearBuffer(void);

	/**
	* @brief	������ �Է�
	* @details
	* @param	const char*(������ ������), int(ũ��)
	* @return	int(�Էµ� ������ ũ��)
	**/
	int	PutData(const char* input, int size);

	/**
	* @brief	������ ���
	* @details
	* @param	char*(������ ������), int(ũ��)
	* @return	int(��µ� ������ ũ��)
	**/
	int	GetData(char* output, int size);
protected:
	/**
	* @brief	��Ŷ ������ ���
	* @details
	* @param	void
	* @return	int(��Ŷ ������)
	**/
	int GetPacketSize(void);

	/**
	* @brief	���ϴ� ���̸�ŭ �б���ġ���� ���� / ������ġ �̵�
	* @details
	* @param	int(���ϴ� ����)
	* @return	void
	**/
	void MoveFront(int size);
	void MoveRear(int size);

	/**
	* @brief	������ Front ������ ����
	* @details
	* @param	void
	* @return	char*(���� ������)
	**/
	char* GetFrontBufferPtr(void);

	/**
	* @brief	������ Rear ������ ����
	* @details
	* @param	void
	* @return	char*(���� ������)
	**/
	char* GetRearBufferPtr(void);

	/**
	* @brief	������ Header ������ ����
	* @details
	* @param	void
	* @return	char*(���� ������)
	**/
	char* GetHeaderPtr(void);

	/**
	* @brief	��� �Է�
	* @details
	* @param	const char*(��� ������), int(ũ��)
	* @return	int(�Էµ� ��� ũ��)
	**/
	int PutHeader(const char* header, int size);

	/**
	* @brief	������ ���� ī��Ʈ ����
	* @details
	* @param	void
	* @return	int(������ ���� ī��Ʈ ��)
	**/
	int IncrementRefCount(void);

	/**
	* @brief	������ ���� ī��Ʈ ����
	* @details
	* @param	void
	* @return	int(���ҵ� ���� ī��Ʈ ��)
	**/
	int DecrementRefCount(void);
public:
	NetPacket& operator = (const NetPacket& packet);

	NetPacket& operator << (const char value);
	NetPacket& operator << (const unsigned char value);

	NetPacket& operator << (const short value);
	NetPacket& operator << (const unsigned short value);

	NetPacket& operator << (const long value);
	NetPacket& operator << (const unsigned long value);

	NetPacket& operator << (const long long value);
	NetPacket& operator << (const unsigned long long value);

	NetPacket& operator << (const int value);
	NetPacket& operator << (const unsigned int value);

	NetPacket& operator << (const float value);
	NetPacket& operator << (const double value);

	NetPacket& operator >> (char& value);
	NetPacket& operator >> (unsigned char& value);

	NetPacket& operator >> (short& value);
	NetPacket& operator >> (unsigned short& value);

	NetPacket& operator >> (long& value);
	NetPacket& operator >> (unsigned long& value);

	NetPacket& operator >> (long long& value);
	NetPacket& operator >> (unsigned long long& value);

	NetPacket& operator >> (int& value);
	NetPacket& operator >> (unsigned int& value);

	NetPacket& operator >> (float& value);
	NetPacket& operator >> (double& value);
protected:
	char* _buffer;
	char* _bufferEnd;
	char* _front;
	char* _rear;
	char* _header;
	int _bufferSize;
	long _refCount;
	static ObjectPool<NetPacket> _packetPool;
	friend class ObjectPool<NetPacket>;
	friend class NetPacketPtr;
	friend class LanServer;
};
JAYNAMESPACEEND

#endif

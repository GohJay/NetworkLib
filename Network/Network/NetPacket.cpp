#include "NetPacket.h"
#include "NetException.h"
#include "Error.h"
#include "Protocol.h"

#define PACKET_HEADER_SIZE		8

using namespace Jay;

ObjectPool_TLS<NetPacket> NetPacket::_packetPool(0, false);
NetPacket::NetPacket(int bufferSize) : _bufferSize(bufferSize), _refCount(0)
{
	_buffer = (char*)malloc(_bufferSize + PACKET_HEADER_SIZE);
	_bufferEnd = _buffer + _bufferSize + PACKET_HEADER_SIZE;
	_front = _buffer + PACKET_HEADER_SIZE;
	_rear = _buffer + PACKET_HEADER_SIZE;
	_header = _front;
}
NetPacket::~NetPacket()
{
	free(_buffer);
}
NetPacket* NetPacket::Alloc(void)
{
	NetPacket* packet = _packetPool.Alloc();
	packet->ClearBuffer();
	packet->_refCount = 1;
	packet->_encode = false;
	return packet;
}
void NetPacket::Free(NetPacket* packet)
{
	if (InterlockedDecrement(&packet->_refCount) == 0)
		_packetPool.Free(packet);
}
long NetPacket::IncrementRefCount(void)
{
	return InterlockedIncrement(&_refCount);
}
int NetPacket::GetBufferSize(void)
{
	return _bufferSize;
}
int NetPacket::GetFreeSize(void)
{
	return _bufferEnd - _rear;
}
int NetPacket::GetUseSize(void)
{
	return _rear - _front;
}
int NetPacket::GetRefCount(void)
{
	return _refCount;
}
void NetPacket::Resize(int bufferSize)
{
	if (_bufferSize >= bufferSize)
		return;

	char* buffer = (char*)malloc(bufferSize + PACKET_HEADER_SIZE);
	char* rear = buffer + (_rear - _buffer);
	char* front = buffer + (_front - _buffer);
	memmove(front, _front, GetUseSize());
	_rear = rear;
	_front = front;
	_header = _front;
	free(_buffer);
	_buffer = buffer;
	_bufferEnd = _buffer + bufferSize + PACKET_HEADER_SIZE;
	_bufferSize = bufferSize;
}
void NetPacket::ClearBuffer(void)
{
	_front = _buffer + PACKET_HEADER_SIZE;
	_rear = _buffer + PACKET_HEADER_SIZE;
	_header = _front;
}
int NetPacket::PutData(const char * input, int size)
{
	int freeSize = GetFreeSize();
	if (size > freeSize)
		size = freeSize;
	memmove(_rear, input, size);
	MoveRear(size);
	return size;
}
int NetPacket::GetData(char * output, int size)
{
	int useSize = GetUseSize();
	if (size > useSize)
		size = useSize;
	memmove(output, _front, size);
	MoveFront(size);
	return size;
}
int NetPacket::GetPacketSize(void)
{
	return _rear - _header;
}
void NetPacket::MoveFront(int size)
{
	_front += size;
}
void NetPacket::MoveRear(int size)
{
	_rear += size;
}
char* NetPacket::GetFrontBufferPtr(void)
{
	return _front;
}
char* NetPacket::GetRearBufferPtr(void)
{
	return _rear;
}
char* NetPacket::GetHeaderPtr(void)
{
	return _header;
}
int NetPacket::PutHeader(const char* header, int size)
{
	int freeSize = _front - _buffer;
	if (size > freeSize)
		size = freeSize;
	_header = _front - size;
	memmove(_header, header, size);
	return size;
}
void NetPacket::Encode(BYTE code, BYTE key)
{
	if (_encode)
		return;

	NET_PACKET_HEADER header;
	header.code = code;
	header.len = GetUseSize();
	header.randKey = rand() % 256;
	header.checkSum = MakeChecksum();

	PutHeader((char*)&header, sizeof(NET_PACKET_HEADER));
	Encrypt(key);

	_encode = true;
}
bool NetPacket::Decode(BYTE code, BYTE key)
{
	NET_PACKET_HEADER* header = (NET_PACKET_HEADER*)GetHeaderPtr();
	if (header->code != code)
		return false;

	Decrypt(key);

	BYTE checkSum = MakeChecksum();
	if (header->checkSum != checkSum)
		return false;

	return true;
}
void NetPacket::Encrypt(BYTE key)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 암호화 대상				(checkSum + Payload)
	// 상호 고정키 1Byte - K		(클라 - 서버 쌍방의 상수값)
	// 공개 랜덤키 1Byte - RK	(클라 - 서버 송수신 패킷 헤더에 포함)
	// 
	// 원본 데이터 바이트 단위  D1 D2 D3 D4
	// |-----------------------------------------------------------------------------------------------------------|
	// |           D1           |            D2             |            D3             |             D4           |	
	// |-----------------------------------------------------------------------------------------------------------|
	// |   D1 ^ (RK + 1) = P1   |  D2 ^ (P1 + RK + 2) = P2  |  D3 ^ (P2 + RK + 3) = P3  |  D4 ^ (P3 + RK + 4) = P4 |
	// |   P1 ^ (K + 1) = E1    |  P2 ^ (E1 + K + 2) = E2   |  P3 ^ (E2 + K + 3) = E3   |  P4 ^ (E3 + K + 4) = E4  |
	// 
	// 암호 데이터 바이트 단위  E1 E2 E3 E4
	// ------------------------------------------------------------------------------------------------------------|
	// |		   E1           |            E2             |            E3             |             E4           |
	// |-----------------------------------------------------------------------------------------------------------|
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	NET_PACKET_HEADER* header = (NET_PACKET_HEADER*)GetHeaderPtr();
	BYTE* byte = (BYTE*)&header->checkSum;
	BYTE P1 = 0;
	BYTE E1 = 0;

	int size = sizeof(header->checkSum) + GetUseSize();
	for (int i = 1; i <= size; i++)
	{
		*byte ^= (P1 + header->randKey + i);
		P1 = *byte;

		*byte ^= (E1 + key + i);
		E1 = *byte;

		byte++;
	}
}
void NetPacket::Decrypt(BYTE key)
{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 복호화 대상				(checkSum + Payload)
	// 상호 고정키 1Byte - K		(클라 - 서버 쌍방의 상수값)
	// 공개 랜덤키 1Byte - RK	(클라 - 서버 송수신 패킷 헤더에 포함)
	// 
	// 암호 데이터 바이트 단위  E1 E2 E3 E4
	// ------------------------------------------------------------------------------------------------------------|
	// |		   E1           |            E2             |            E3             |             E4           |
	// |-----------------------------------------------------------------------------------------------------------|
	// 
	// 원본 데이터 바이트 단위  D1 D2 D3 D4
	// |-----------------------------------------------------------------------------------------------------------|
	// |           D1           |            D2             |            D3             |             D4           |		
	// |-----------------------------------------------------------------------------------------------------------|
	// |   E1 ^ (K + 1) = P1    |  E2 ^ (E1 + K + 2) = P2   |  E3 ^ (E2 + K + 3) = P3   |  E4 ^ (E3 + K + 4) = P4  |
	// |   P1 ^ (RK + 1) = D1   |  P2 ^ (P1 + RK + 2) = D2  |  P3 ^ (P2 + RK + 3) = D3  |  P4 ^ (P3 + RK + 4) = D4 |
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	NET_PACKET_HEADER* header = (NET_PACKET_HEADER*)GetHeaderPtr();
	BYTE* byte = (BYTE*)&header->checkSum;
	BYTE E1 = 0;
	BYTE P1 = 0;
	BYTE temp;

	int size = sizeof(header->checkSum) + GetUseSize();
	for (int i = 1; i <= size; i++)
	{
		temp = *byte;
		*byte ^= (E1 + key + i);
		E1 = temp;

		temp = *byte;
		*byte ^= (P1 + header->randKey + i);
		P1 = temp;

		byte++;
	}
}
unsigned char NetPacket::MakeChecksum(void)
{
	BYTE* byte = (BYTE*)GetFrontBufferPtr();
	int size = GetUseSize();
	int checkSum = 0;
	for (int i = 1; i <= size; i++)
	{
		checkSum += *byte;
		byte++;
	}
	return checkSum % 256;
}
NetPacket & NetPacket::operator=(const NetPacket & packet)
{
	free(_buffer);
	_bufferSize = packet._bufferSize;
	_buffer = (char*)malloc(_bufferSize + PACKET_HEADER_SIZE);
	_bufferEnd = _buffer + _bufferSize + PACKET_HEADER_SIZE;
	_rear = _buffer + (packet._rear - packet._buffer);
	_front = _buffer + (packet._front - packet._buffer);
	_header = _front;
	_refCount = packet._refCount;
	_encode = packet._encode;
	memmove(_front, packet._front, packet._rear - packet._front);
	return *this;
}
NetPacket & NetPacket::operator<<(const char value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned char value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const short value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned short value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const long value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned long value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const long long value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned long long value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const int value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned int value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const float value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const double value)
{
	if (GetFreeSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (char &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned char &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (short & value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned short &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (long &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned long &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (long long &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned long long &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (int &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned int &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (float &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (double &value)
{
	if (GetUseSize() < sizeof(value))
		throw NetException(NET_ERROR_NOT_ENOUGH_SPACE);

	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}

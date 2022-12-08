#include "NetPacket.h"

#define PACKET_HEADER_SIZE	10

USEJAYNAMESPACE
ObjectPool<NetPacket> NetPacket::_packetPool(0, false);
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
	return packet;
}
void NetPacket::Free(NetPacket* packet)
{
	_packetPool.Free(packet);
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
int NetPacket::IncrementRefCount(void)
{
	return InterlockedIncrement(&_refCount);
}
int NetPacket::DecrementRefCount(void)
{
	return InterlockedDecrement(&_refCount);
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
	memmove(_front, packet._front, packet._rear - packet._front);
	return *this;
}
NetPacket & NetPacket::operator<<(const char value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned char value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const short value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned short value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const long long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned long long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const int value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const unsigned int value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const float value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator<<(const double value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (char &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned char &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (short & value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned short &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (long long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned long long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (int &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (unsigned int &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (float &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
NetPacket & NetPacket::operator >> (double &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}

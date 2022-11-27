#include "SerializationBuffer.h"

#define PACKET_HEADER_SIZE	10

USEJAYNAMESPACE
ObjectPool<SerializationBuffer> SerializationBuffer::_packetPool(0, false);
SerializationBuffer::SerializationBuffer(int bufferSize) : _bufferSize(bufferSize)
{
	_buffer = (char*)malloc(_bufferSize + PACKET_HEADER_SIZE);
	_bufferEnd = _buffer + _bufferSize + PACKET_HEADER_SIZE;
	_front = _buffer + PACKET_HEADER_SIZE;
	_rear = _buffer + PACKET_HEADER_SIZE;
	_header = _front;
}
SerializationBuffer::~SerializationBuffer()
{
	free(_buffer);
}
SerializationBuffer* SerializationBuffer::Alloc(void)
{
	SerializationBuffer* packet = _packetPool.Alloc();
	packet->ClearBuffer();
	return packet;
}
void SerializationBuffer::Free(SerializationBuffer* packet)
{
	_packetPool.Free(packet);
}
int SerializationBuffer::GetBufferSize(void)
{
	return _bufferSize;
}
int SerializationBuffer::GetFreeSize(void)
{
	return _bufferEnd - _rear;
}
int SerializationBuffer::GetUseSize(void)
{
	return _rear - _front;
}
void SerializationBuffer::Resize(int bufferSize)
{
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
void SerializationBuffer::ClearBuffer(void)
{
	_front = _buffer + PACKET_HEADER_SIZE;
	_rear = _buffer + PACKET_HEADER_SIZE;
	_header = _front;
}
int SerializationBuffer::PutData(const char * input, int size)
{
	int freeSize = GetFreeSize();
	if (size > freeSize)
		size = freeSize;
	memmove(_rear, input, size);
	MoveRear(size);
	return size;
}
int SerializationBuffer::GetData(char * output, int size)
{
	int useSize = GetUseSize();
	if (size > useSize)
		size = useSize;
	memmove(output, _front, size);
	MoveFront(size);
	return size;
}
int SerializationBuffer::GetPacketSize(void)
{
	return _rear - _header;
}
void SerializationBuffer::MoveFront(int size)
{
	_front += size;
}
void SerializationBuffer::MoveRear(int size)
{
	_rear += size;
}
char* SerializationBuffer::GetFrontBufferPtr(void)
{
	return _front;
}
char* SerializationBuffer::GetRearBufferPtr(void)
{
	return _rear;
}
char* SerializationBuffer::GetHeaderPtr(void)
{
	return _header;
}
int SerializationBuffer::PutHeader(const char* header, int size)
{
	int freeSize = _front - _buffer;
	if (size > freeSize)
		size = freeSize;
	_header = _front - size;
	memmove(_header, header, size);
	return size;
}
SerializationBuffer & SerializationBuffer::operator=(const SerializationBuffer & packet)
{
	free(_buffer);
	_bufferSize = packet._bufferSize;
	_buffer = (char*)malloc(_bufferSize + PACKET_HEADER_SIZE);
	_bufferEnd = _buffer + _bufferSize + PACKET_HEADER_SIZE;
	_rear = _buffer + (packet._rear - packet._buffer);
	_front = _buffer + (packet._front - packet._buffer);
	_header = _front;
	memmove(_front, packet._front, packet._rear - packet._front);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const char value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const unsigned char value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const wchar_t value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const short value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const unsigned short value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const unsigned long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const long long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const unsigned long long value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const int value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const unsigned int value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const float value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(const double value)
{
	memmove(_rear, (char*)&value, sizeof(value));
	MoveRear(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (char &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned char &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (const wchar_t value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (short & value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned short &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (long long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned long long &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (int &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned int &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (float &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (double &value)
{
	memmove((char*)&value, _front, sizeof(value));
	MoveFront(sizeof(value));
	return *this;
}

#include "SerializationBuffer.h"

USEJAYNAMESPACE
SerializationBuffer::SerializationBuffer(int bufferSize) : _bufferSize(bufferSize)
{
	_buffer = (char*)malloc(_bufferSize);
	_bufferEnd = _buffer + _bufferSize;
	_front = _buffer;
	_rear = _buffer;
}
SerializationBuffer::~SerializationBuffer()
{
	free(_buffer);
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
	char* buffer = (char*)malloc(bufferSize);
	char* rear = buffer + (_rear - _buffer);
	char* front = buffer + (_front - _buffer);
	memmove(front, _front, GetUseSize());
	_rear = rear;
	_front = front;
	free(_buffer);
	_buffer = buffer;
	_bufferEnd = _buffer + bufferSize;
	_bufferSize = bufferSize;
}
void SerializationBuffer::MoveFront(int size)
{
	_front += size;
}
void SerializationBuffer::MoveRear(int size)
{
	_rear += size;
}
void SerializationBuffer::ClearBuffer(void)
{
	_front = _buffer;
	_rear = _buffer;
}
char * SerializationBuffer::GetBufferPtr(void)
{
	return _buffer;
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
SerializationBuffer & SerializationBuffer::operator=(SerializationBuffer & packet)
{
	free(_buffer);
	_bufferSize = packet._bufferSize;
	_buffer = (char*)malloc(_bufferSize);
	_bufferEnd = _buffer + _bufferSize;
	_rear = _buffer + (packet._rear - packet._buffer);
	_front = _buffer + (packet._front - packet._buffer);
	memmove(_front, packet._front, packet.GetUseSize());
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(char value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(unsigned char value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(short value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(unsigned short value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(long value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(unsigned long value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(long long value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(unsigned long long value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(int value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(unsigned int value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(float value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator<<(double value)
{
	int size = sizeof(value);
	memmove(_rear, (char*)&value, size);
	MoveRear(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (char &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned char &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (short & value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned short &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (long &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned long &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (long long &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned long long &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (int &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (unsigned int &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (float &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}
SerializationBuffer & SerializationBuffer::operator >> (double &value)
{
	int size = sizeof(value);
	memmove((char*)&value, _front, size);
	MoveFront(size);
	return *this;
}

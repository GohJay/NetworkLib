#include "RingBuffer.h"

USEJAYNAMESPACE
RingBuffer::RingBuffer(int bufferSize) : _bufferSize(bufferSize)
{
	_buffer = (char*)malloc(_bufferSize);
	_bufferEnd = _buffer + _bufferSize;
	_front = _buffer;
	_rear = _buffer;
}
RingBuffer::~RingBuffer()
{
	free(_buffer);
}
int RingBuffer::GetFreeSize(void)
{
	int diff = _front - GetRearBufferPtr();
	if (diff >= 0)
		return diff;
	else
		return _bufferSize + diff;
}
int RingBuffer::GetUseSize(void)
{
	int diff = GetRearBufferPtr() - GetFrontBufferPtr();
	if (diff >= 0)
		return diff;
	else
		return _bufferSize + diff;
}
int RingBuffer::DirectEnqueueSize(void)
{
	char* rear = GetRearBufferPtr();
	int diff = _front - rear;
	if (diff >= 0)
		return diff;
	else
		return _bufferEnd - rear;
}
int RingBuffer::DirectDequeueSize(void)
{
	char* front = GetFrontBufferPtr();
	int diff = GetRearBufferPtr() - front;
	if (diff >= 0)
		return diff;
	else
		return _bufferEnd - front;
}
int RingBuffer::Enqueue(const char * input, int size)
{
	char* rear = GetRearBufferPtr();
	int freeSize;
	int directSize;
	int diff = _front - rear;
	if (diff >= 0)
	{
		freeSize = diff;
		directSize = diff;
	}
	else
	{
		freeSize = _bufferSize + diff;
		directSize = _bufferEnd - rear;
	}
	
	if (size > freeSize)
		size = freeSize;

	if (directSize >= size)
		memmove(rear, input, size);
	else
	{
		memmove(rear, input, directSize);
		memmove(_buffer, input + directSize, size - directSize);
	}
	MoveRear(size);
	return size;
}
int RingBuffer::Dequeue(char * output, int size)
{
	char* front = GetFrontBufferPtr();
	int useSize;
	int directSize;
	int diff = GetRearBufferPtr() - front;
	if (diff >= 0)
	{
		useSize = diff;
		directSize = diff;
	}
	else
	{
		useSize = _bufferSize + diff;
		directSize = _bufferEnd - front;
	}

	if (size > useSize)
		size = useSize;

	if (directSize >= size)
		memmove(output, front, size);
	else
	{
		memmove(output, front, directSize);
		memmove(output + directSize, _buffer, size - directSize);
	}
	MoveFront(size);
	return size;
}
int RingBuffer::Peek(char * output, int size)
{
	char* front = GetFrontBufferPtr();
	int useSize;
	int directSize;
	int diff = GetRearBufferPtr() - front;
	if (diff >= 0)
	{
		useSize = diff;
		directSize = diff;
	}
	else
	{
		useSize = _bufferSize + diff;
		directSize = _bufferEnd - front;
	}
	
	if (size > useSize)
		size = useSize;

	if (directSize >= size)
		memmove(output, front, size);
	else
	{
		memmove(output, front, directSize);
		memmove(output + directSize, _buffer, size - directSize);
	}
	return size;
}
void RingBuffer::ClearBuffer(void)
{
	_front = _buffer;
	_rear = _buffer;
}
void RingBuffer::MoveFront(int size)
{
	_front = ((_front + size - _buffer) % _bufferSize) + _buffer;
}
void RingBuffer::MoveRear(int size)
{
	_rear = ((_rear + size - _buffer) % _bufferSize) + _buffer;
}
char * RingBuffer::GetFrontBufferPtr(void)
{
	return ((_front + 1 - _buffer) % _bufferSize) + _buffer;
}
char * RingBuffer::GetRearBufferPtr(void)
{
	return ((_rear + 1 - _buffer) % _bufferSize) + _buffer;
}
char * RingBuffer::GetBufferPtr(void)
{
	return _buffer;
}

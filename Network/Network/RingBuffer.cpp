#include "RingBuffer.h"

USEJAYNAMESPACE
RingBuffer::RingBuffer(int bufferSize) : _bufferSize(bufferSize)
{
	_buffer = new char[_bufferSize + 1];
	_bufferEnd = _buffer + _bufferSize + 1;
	_front = _buffer;
	_rear = _buffer;
}
RingBuffer::~RingBuffer()
{
	delete[] _buffer;
}
int RingBuffer::GetFreeSize(void)
{
	int diff = _front - _rear;
	if (diff > 0)
		return diff - 1;
	else
		return _bufferSize + diff;
}
int RingBuffer::GetUseSize(void)
{
	int diff = _rear - _front;
	if (diff >= 0)
		return diff;
	else
		return _bufferSize + diff + 1;
}
int RingBuffer::DirectEnqueueSize(void)
{
	int diff = _front - _rear;
	if (diff > 0)
		return diff - 1;
	else
		return _bufferEnd - _rear - 1;
}
int RingBuffer::DirectDequeueSize(void)
{
	int diff = _rear - _front;
	if (diff >= 0)
		return diff;
	else
		return _bufferEnd - _front - 1;
}
int RingBuffer::Enqueue(const char * input, int size)
{
	int freeSize;
	int directSize;
	int diff = _front - _rear;
	if (diff > 0)
	{
		freeSize = diff - 1;
		directSize = diff - 1;
	}
	else
	{
		freeSize = _bufferSize + diff;
		directSize = _bufferEnd - _rear - 1;
	}
	
	if (size > freeSize)
		size = freeSize;

	if (directSize >= size)
		memmove_s(_rear + 1, directSize, input, size);
	else
	{
		memmove_s(_rear + 1, directSize, input, directSize);
		memmove_s(_buffer, size - directSize, input + directSize, size - directSize);
	}
	MoveRear(size);
	return size;
}
int RingBuffer::Dequeue(char * output, int size)
{
	int useSize;
	int directSize;
	int diff = _rear - _front;
	if (diff >= 0)
	{
		useSize = diff;
		directSize = diff;
	}
	else
	{
		useSize = _bufferSize + diff + 1;
		directSize = _bufferEnd - _front - 1;
	}

	if (size > useSize)
		size = useSize;

	if (directSize >= size)
		memmove_s(output, size, _front + 1, size);
	else
	{
		memmove_s(output, size, _front + 1, directSize);
		memmove_s(output + directSize, size - directSize, _buffer, size - directSize);
	}
	MoveFront(size);
	return size;
}
int RingBuffer::Peek(char * output, int size)
{
	int useSize;
	int directSize;
	int diff = _rear - _front;
	if (diff >= 0)
	{
		useSize = diff;
		directSize = diff;
	}
	else
	{
		useSize = _bufferSize + diff + 1;
		directSize = _bufferEnd - _front - 1;
	}
	
	if (size > useSize)
		size = useSize;

	if (directSize >= size)
		memmove_s(output, size, _front + 1, size);
	else
	{
		memmove_s(output, size, _front + 1, directSize);
		memmove_s(output + directSize, size - directSize, _buffer, size - directSize);
	}
	return size;
}
void RingBuffer::MoveRear(int size)
{
	int directSize = _bufferEnd - _rear - 1;
	if (directSize >= size)
		_rear += size;
	else
		_rear = _buffer - 1 + size - directSize;
}
void RingBuffer::MoveFront(int size)
{
	int directSize = _bufferEnd - _front - 1;
	if (directSize >= size)
		_front += size;
	else
		_front = _buffer - 1 + size - directSize;
}
void RingBuffer::ClearBuffer(void)
{
	_rear = _buffer;
	_front = _buffer;
}

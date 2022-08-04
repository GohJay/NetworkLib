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
	return (_front <= _rear) ? _bufferSize + _front - _rear : _front - _rear - 1;
}
int RingBuffer::GetUseSize(void)
{
	return (_front <= _rear) ? _rear - _front : _bufferSize + _front - _rear;
}
int RingBuffer::DirectEnqueueSize(void)
{
	return (_front <= _rear) ? _bufferEnd - _rear - 1 : _front - _rear - 1;
}
int RingBuffer::DirectDequeueSize(void)
{
	return (_front <= _rear) ? _rear - _front : _bufferEnd - _front - 1;
}
bool RingBuffer::Enqueue(const char * input, int size)
{
	int freeSize = GetFreeSize();
	if (freeSize >= size)
	{
		int directSize = DirectEnqueueSize();
		if (directSize >= size)
		{
			memmove_s(_rear + 1, directSize, input, size);
			_rear += size;
			return true;
		}
		memmove_s(_rear + 1, directSize, input, directSize);
		input += directSize;
		memmove_s(_buffer, size - directSize, input, size - directSize);
		_rear = _buffer + size - directSize - 1;
		return true;
	}
	return false;
}
bool RingBuffer::Dequeue(char * output, int size)
{
	int useSize = GetUseSize();
	if (useSize >= size)
	{
		int directSize = DirectDequeueSize();
		if (directSize >= size)
		{
			memmove_s(output, size, _front + 1, size);
			_front += size;
			return true;
		}
		memmove_s(output, size, _front + 1, directSize);
		output += directSize;
		memmove_s(output, size - directSize, _buffer, size - directSize);
		_front = _buffer + size - directSize - 1;
		return true;
	}
	return false;
}
bool RingBuffer::Peek(char * output, int size)
{
	int useSize = GetUseSize();
	if (useSize >= size)
	{
		int directSize = DirectDequeueSize();
		if (directSize >= size)
		{
			memmove_s(output, size, _front + 1, size);
			return true;
		}
		memmove_s(output, size, _front + 1, directSize);
		output += directSize;
		memmove_s(output, size - directSize, _buffer, size - directSize);
		return true;
	}
	return false;
}
void RingBuffer::ClearBuffer(void)
{
	_front = _buffer;
	_rear = _buffer;
}

#include "../Network/RingBuffer.h"
#include "../Network/SerializationBuffer.h"
#include <iostream>
#include <thread>
#pragma comment(lib, "Network.lib")

const char* data = "1234567890 abcdefghijklmnopqrstuvwxyz 1234567890 abcdefghijklmnopqrstuvwxyz 123451234567890 abcdefghijklmnopqrstuvwxyz 1";
int size = strlen(data);
Jay::RingBuffer g_RingBuffer(200);

void SerializationTest()
{
	Jay::SerializationBuffer packet(512);
	packet.PutData(data, size);

	char buffer[512];
	packet.GetData(buffer, size);
	buffer[size] = '\0';
	std::cout << buffer << std::endl;
}
void SingleTest()
{
	srand(3);
	g_RingBuffer.Enqueue(data, size);
	char buffer1[128];
	char buffer2[128];
	for (;;)
	{
		int randSize = rand() % size;
		if (g_RingBuffer.Peek(buffer1, randSize) != randSize)
		{
			std::cout << "Peek error" << std::endl;
			break;
		}
		if (g_RingBuffer.Dequeue(buffer2, randSize) != randSize)
		{
			std::cout << "Dequeue error" << std::endl;
			break;
		}
		if (memcmp(buffer1, buffer2, randSize) != 0)
		{
			std::cout << "Not same" << std::endl;
			break;
		}
		if (g_RingBuffer.Enqueue(buffer1, randSize) != randSize)
		{
			std::cout << "Enqueue error" << std::endl;
			break;
		}
		buffer1[randSize] = '\0';
		std::cout << buffer1;
		Sleep(200);
	}
}
void EnqueueTest()
{
	for (;;)
	{
		if (g_RingBuffer.GetFreeSize() >= size)
			g_RingBuffer.Enqueue(data, size);
		Sleep(200);
	}
}
void DequeueTest()
{
	for (;;)
	{
		int randSize = rand() % size;
		char buffer[128];
		int ret = g_RingBuffer.Dequeue(buffer, randSize);
		buffer[ret] = '\0';
		std::cout << buffer;
		Sleep(200);
	}
}

int main()
{
	SerializationTest();
	return 0;

	//SingleTest();
	//return 0;
	//
	//std::thread enqueue_thread(EnqueueTest);
	//std::thread dequeue_thread(DequeueTest);
	//enqueue_thread.join();
	//dequeue_thread.join();
    //return 0;
}

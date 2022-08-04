#include "../Network/RingBuffer.h"
#include <iostream>
#pragma comment(lib, "Network.lib")

int main()
{
	Jay::RingBuffer ringBuffer;
	const char* data = "1234567890 abcdefghijklmnopqrstuvwxyz 1234567890 abcdefghijklmnopqrstuvwxyz 123451234567890 abcdefghijklmnopqrstuvwxyz 1";
	int size = strlen(data);
	ringBuffer.Enqueue(data, size);

	char buffer1[128];
	char buffer2[128];
	for (;;)
	{
		int randSize = rand() % size;
		if (!ringBuffer.Peek(buffer1, randSize))
		{
			std::cout << "Peek error" << std::endl;
			break;
		}
		if (!ringBuffer.Dequeue(buffer2, randSize))
		{
			std::cout << "Dequeue error" << std::endl;
			break;
		}
		if (memcmp(buffer1, buffer2, randSize) != 0)
		{
			std::cout << "Not same" << std::endl;
			break;
		}
		if (!ringBuffer.Enqueue(buffer1, randSize))
		{
			std::cout << "Enqueue error" << std::endl;
			break;
		}
		buffer1[randSize] = '\0';
		std::cout << buffer1;
		Sleep(200);
	}
    return 0;
}

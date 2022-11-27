#include "EchoServer.h"
#include <conio.h>
#include <time.h>
#include <iostream>
#include <Windows.h>
#pragma comment(lib, "Network.lib")

EchoServer g_Server;
bool g_StopSignal = false;

void Control()
{
	wchar_t controlKey;
	if (_kbhit())
	{
		controlKey = _getwch();
		if ((controlKey == L'q' || controlKey == L'Q'))
		{
			g_StopSignal = true;
		}
	}
}
void Monitor()
{
	tm stTime;
	time_t timer;
	timer = time(NULL);
	localtime_s(&stTime, &timer);

	wprintf_s(L"\
[%d/%02d/%02d %02d:%02d:%02d]\n\
------------------------------------\n\
Session Count: %d\n\
------------------------------------\n\
Accept TPS: %d\n\
Recv TPS: %d\n\
Send TPS: %d\n\
------------------------------------\n\
\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
		, stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec
		, g_Server.GetSessionCount()
		, g_Server.GetAcceptTPS()
		, g_Server.GetRecvTPS()
		, g_Server.GetSendTPS());
}
int main()
{
	wchar_t ip[16] = L"0.0.0.0";
	int port = 6000;
	int workerCreateCnt = 7;
	int workerRunningCnt = 0;
	WORD sessionMax = 10000;
	if (!g_Server.Start(ip, port, workerCreateCnt, workerRunningCnt, sessionMax))
		return 0;

	while (!g_StopSignal)
	{
		Control();
		Monitor();
		Sleep(1000);
	}

	g_Server.Stop();
	return 0;
}

#include "stdafx.h"
#include "EchoServer.h"
#include "EchoServerEx.h"
#include <conio.h>
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Network.lib")

//EchoServer g_EchoServer;
EchoServerEx g_EchoServer;
LoginServerEx g_LoginServer(&g_EchoServer);
GameServerEx g_GameServer(&g_EchoServer);
bool g_StopSignal = false;

void Run();
void Monitor();
void Control();

int main()
{
	timeBeginPeriod(1);

	Run();

	wprintf_s(L"Press any key to continue . . . ");
	_getwch();

	timeEndPeriod(1);
	return 0;
}

void Run()
{
	wchar_t ip[16] = L"0.0.0.0";
	int port = 40000;
	int workerCreateCnt = 3;
	int workerRunningCnt = 2;
	WORD sessionMax = 6000;
	BYTE packetCode = 119;
	BYTE packetKey = 50;

	if (!g_EchoServer.Start(ip, port, workerCreateCnt, workerRunningCnt, sessionMax, packetCode, packetKey))
		return;

	while (!g_StopSignal)
	{
		Control();
		Monitor();
		Sleep(1000);
	}

	g_EchoServer.Stop();
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
PacketPool Use: %d\n\
------------------------------------\n\
Total Accept: %lld\n\
Accept TPS: %d\n\
Recv TPS: %d\n\
Send TPS: %d\n\
------------------------------------\n\
Auth FPS: %d\n\
Game FPS: %d\n\
------------------------------------\n\
\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
		, stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec
		, g_EchoServer.GetSessionCount()
		, g_EchoServer.GetUsePacketPool()
		, g_EchoServer.GetTotalAcceptCount()
		, g_EchoServer.GetAcceptTPS()
		, g_EchoServer.GetRecvTPS()
		, g_EchoServer.GetSendTPS()
		, g_LoginServer.GetFPS()
		, g_GameServer.GetFPS());
		//, g_EchoServer.GetFPS());
}

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

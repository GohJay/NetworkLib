#include "stdafx.h"
#include "EchoServer.h"

EchoServer::EchoServer()
{
}
EchoServer::~EchoServer()
{
}
bool EchoServer::OnConnectionRequest(const wchar_t* ipaddress, int port)
{
	return true;
}
void EchoServer::OnClientJoin(DWORD64 sessionID)
{
	__int64 login = 0x7fffffffffffffff;

	Jay::NetPacketPtr sc_packet;
	sc_packet << login;
	SendPacket(sessionID, sc_packet);
}
void EchoServer::OnClientLeave(DWORD64 sessionID)
{
}
void EchoServer::OnRecv(DWORD64 sessionID, Jay::NetPacketPtr packet)
{
	__int64 echo;
	packet >> echo;

	Jay::NetPacketPtr sc_packet;
	sc_packet << echo;
	SendPacket(sessionID, sc_packet);
}
void EchoServer::OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam)
{
	tm stTime;
	time_t timer = time(NULL);
	localtime_s(&stTime, &timer);

	wchar_t logFile[MAX_PATH];
	swprintf_s(logFile, L"Dev_%d-%02d-%02d.log", stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday);

	FILE* pFile;
	if (_wfopen_s(&pFile, logFile, L"at") != 0)
		return;
	fwprintf_s(pFile
		, L"[%d/%02d/%02d %02d:%02d:%02d] %s() line: %d - error: %d, wParam: %llu, lParam: %llu\n"
		, stTime.tm_year + 1900
		, stTime.tm_mon + 1
		, stTime.tm_mday
		, stTime.tm_hour
		, stTime.tm_min
		, stTime.tm_sec
		, funcname
		, linenum
		, errcode
		, wParam
		, lParam);
	fclose(pFile);
}

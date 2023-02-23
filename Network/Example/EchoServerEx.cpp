#include "stdafx.h"
#include "EchoServerEx.h"
#include "CommonProtocol.h"

EchoServerEx::EchoServerEx()
{
}
EchoServerEx::~EchoServerEx()
{
}
bool EchoServerEx::OnConnectionRequest(const wchar_t* ipaddress, int port)
{
	return true;
}
void EchoServerEx::OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam)
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

LoginServerEx::LoginServerEx(EchoServerEx* subject) : _subject(subject), _stopSignal(false)
{
	_subject->AttachContent(this, CONTENT_ID_AUTH, FRAME_INTERVAL_AUTH, true);
	_managementThread = std::thread(&LoginServerEx::ManagementThread, this);
}
LoginServerEx::~LoginServerEx()
{
	_stopSignal = true;
	_managementThread.join();
}
int LoginServerEx::GetFPS()
{
	return _oldFPS;
}
void LoginServerEx::OnUpdate()
{
	_curFPS++;
}
void LoginServerEx::OnClientJoin(DWORD64 sessionID)
{
}
void LoginServerEx::OnClientLeave(DWORD64 sessionID)
{
}
void LoginServerEx::OnContentEnter(DWORD64 sessionID, WPARAM wParam, LPARAM lParam)
{
}
void LoginServerEx::OnContentExit(DWORD64 sessionID)
{
}
void LoginServerEx::OnRecv(DWORD64 sessionID, Jay::NetPacket* packet)
{
	WORD type;
	(*packet) >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		LoginProc(sessionID, packet);
		break;
	default:
		Jay::CrashDump::Crash();
		break;
	}
}
void LoginServerEx::LoginProc(DWORD64 sessionID, Jay::NetPacket* packet)
{
	//--------------------------------------------------------------------
	// Packet Deserialize
	//--------------------------------------------------------------------
	INT64 accountNo;
	char sessionKey[64];
	int version;
	(*packet) >> accountNo;
	if (packet->GetData(sessionKey, sizeof(sessionKey)) != sizeof(sessionKey))
		Jay::CrashDump::Crash();
	(*packet) >> version;

	//--------------------------------------------------------------------
	// Move Content To GameServerEx
	//--------------------------------------------------------------------
	_subject->MoveContent(sessionID, CONTENT_ID_GAME, NULL, NULL);

	//--------------------------------------------------------------------
	// Packet Serialize
	//--------------------------------------------------------------------
	BYTE status = true;

	Jay::NetPacket* resPacket = Jay::NetPacket::Alloc();

	(*resPacket) << (WORD)en_PACKET_CS_GAME_RES_LOGIN;
	(*resPacket) << status;
	(*resPacket) << accountNo;
	_subject->SendPacket(sessionID, resPacket);

	Jay::NetPacket::Free(resPacket);
}
void LoginServerEx::ManagementThread()
{
	while (!_stopSignal)
	{
		Sleep(1000);
		_oldFPS.exchange(_curFPS.exchange(0));
	}
}

GameServerEx::GameServerEx(EchoServerEx* subject) : _subject(subject), _stopSignal(false)
{
	_subject->AttachContent(this, CONTENT_ID_GAME, FRAME_INTERVAL_GAME);
	_managementThread = std::thread(&GameServerEx::ManagementThread, this);
}
GameServerEx::~GameServerEx()
{
	_stopSignal = true;
	_managementThread.join();
}
int GameServerEx::GetFPS()
{
	return _oldFPS;
}
void GameServerEx::OnUpdate()
{
	_curFPS++;
}
void GameServerEx::OnClientJoin(DWORD64 sessionID)
{
}
void GameServerEx::OnClientLeave(DWORD64 sessionID)
{
}
void GameServerEx::OnContentEnter(DWORD64 sessionID, WPARAM wParam, LPARAM lParam)
{
}
void GameServerEx::OnContentExit(DWORD64 sessionID)
{
}
void GameServerEx::OnRecv(DWORD64 sessionID, Jay::NetPacket* packet)
{
	WORD type;
	(*packet) >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_ECHO:
		PacketProc_Echo(sessionID, packet);
		break;
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		break;
	default:
		Jay::CrashDump::Crash();
		break;
	}
}
void GameServerEx::PacketProc_Echo(DWORD64 sessionID, Jay::NetPacket* packet)
{
	//--------------------------------------------------------------------
	// Packet Deserialize
	//--------------------------------------------------------------------
	INT64 accountNo;
	LONGLONG sendTick;
	(*packet) >> accountNo;
	(*packet) >> sendTick;

	//--------------------------------------------------------------------
	// Packet Serialize
	//--------------------------------------------------------------------
	Jay::NetPacket* resPacket = Jay::NetPacket::Alloc();

	(*resPacket) << (WORD)en_PACKET_CS_GAME_RES_ECHO;
	(*resPacket) << accountNo;
	(*resPacket) << sendTick;
	_subject->SendPacket(sessionID, resPacket);

	Jay::NetPacket::Free(resPacket);
}
void GameServerEx::ManagementThread()
{
	while (!_stopSignal)
	{
		Sleep(1000);
		_oldFPS.exchange(_curFPS.exchange(0));
	}
}

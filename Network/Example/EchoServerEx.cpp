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
void EchoServerEx::OnClientJoin(DWORD64 sessionID)
{
}
void EchoServerEx::OnClientLeave(DWORD64 sessionID)
{
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

LoginServer::LoginServer(EchoServerEx* subject) : _subject(subject), _stopSignal(false)
{
	_subject->AttachContent(this, CONTENT_ID_AUTH, FRAME_INTERVAL_AUTH, true);
	_managementThread = std::thread(&LoginServer::ManagementThread, this);
}
LoginServer::~LoginServer()
{
	_stopSignal = true;
	_managementThread.join();
}
int LoginServer::GetFPS()
{
	return _oldFPS;
}
void LoginServer::OnUpdate()
{
	_curFPS++;
}
void LoginServer::OnContentJoin(DWORD64 sessionID)
{
}
void LoginServer::OnContentLeave(DWORD64 sessionID)
{
}
void LoginServer::OnRecv(DWORD64 sessionID, Jay::NetPacket* packet)
{
	//--------------------------------------------------------------------
	// Packet Deserialize
	//--------------------------------------------------------------------
	WORD type;
	(*packet) >> type;
	if (type != en_PACKET_CS_GAME_REQ_LOGIN)
		Jay::CrashDump::Crash();

	INT64 accountNo;
	char sessionKey[64];
	int version;
	(*packet) >> accountNo;
	if (packet->GetData(sessionKey, sizeof(sessionKey)) != sizeof(sessionKey))
		Jay::CrashDump::Crash();

	(*packet) >> version;

	//--------------------------------------------------------------------
	// Move Content To GameServer
	//--------------------------------------------------------------------
	_subject->MoveContent(sessionID, CONTENT_ID_GAME);

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
void LoginServer::ManagementThread()
{
	while (!_stopSignal)
	{
		Sleep(1000);
		_oldFPS.exchange(_curFPS.exchange(0));
	}
}

GameServer::GameServer(EchoServerEx* subject) : _subject(subject), _stopSignal(false)
{
	_subject->AttachContent(this, CONTENT_ID_GAME, FRAME_INTERVAL_GAME);
	_managementThread = std::thread(&GameServer::ManagementThread, this);
}
GameServer::~GameServer()
{
	_stopSignal = true;
	_managementThread.join();
}
int GameServer::GetFPS()
{
	return _oldFPS;
}
void GameServer::OnUpdate()
{
	_curFPS++;
}
void GameServer::OnContentJoin(DWORD64 sessionID)
{
}
void GameServer::OnContentLeave(DWORD64 sessionID)
{
}
void GameServer::OnRecv(DWORD64 sessionID, Jay::NetPacket* packet)
{
	//--------------------------------------------------------------------
	// Packet Deserialize
	//--------------------------------------------------------------------
	WORD type;
	(*packet) >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_ECHO:
		// do nothing
		break;
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		return;
	default:
		Jay::CrashDump::Crash();
		return;
	}

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
void GameServer::ManagementThread()
{
	while (!_stopSignal)
	{
		Sleep(1000);
		_oldFPS.exchange(_curFPS.exchange(0));
	}
}

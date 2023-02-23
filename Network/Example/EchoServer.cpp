#include "stdafx.h"
#include "EchoServer.h"
#include "CommonProtocol.h"

EchoServer::EchoServer() : _playerPool(0), _jobPool(0), _stopSignal(false)
{
	_updateThread = std::thread(&EchoServer::UpdateThread, this);
	_managementThread = std::thread(&EchoServer::ManagementThread, this);
}
EchoServer::~EchoServer()
{
	_stopSignal = true;
	_updateThread.join();
	_managementThread.join();
}
int EchoServer::GetFPS()
{
	return _oldFPS;
}
bool EchoServer::OnConnectionRequest(const wchar_t* ipaddress, int port)
{
	return true;
}
void EchoServer::OnClientJoin(DWORD64 sessionID)
{
}
void EchoServer::OnClientLeave(DWORD64 sessionID)
{
}
void EchoServer::OnRecv(DWORD64 sessionID, Jay::NetPacket* packet)
{
	//--------------------------------------------------------------------
	// Job Queueing To UpdateThread
	//--------------------------------------------------------------------
	JOB* job = _jobPool.Alloc();
	job->type = JOB_TYPE_PACKET_PROC;
	job->sessionID = sessionID;
	job->packet = packet;

	packet->IncrementRefCount();
	_jobQ[sessionID % JOBQUEUE_SIZE].Enqueue(job);
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
void EchoServer::PacketProc(DWORD64 sessionID, Jay::NetPacket* packet)
{
	WORD type;
	(*packet) >> type;

	switch (type)
	{
	case en_PACKET_CS_GAME_REQ_LOGIN:
		PacketProc_Login(sessionID, packet);
		break;
	case en_PACKET_CS_GAME_REQ_ECHO:
		PacketProc_Echo(sessionID, packet);
		break;
	case en_PACKET_CS_GAME_REQ_HEARTBEAT:
		break;
	default:
		Jay::CrashDump::Crash();
		break;
	}

	Jay::NetPacket::Free(packet);
}
void EchoServer::PacketProc_Login(DWORD64 sessionID, Jay::NetPacket* packet)
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
	// Packet Serialize
	//--------------------------------------------------------------------
	BYTE status = true;

	Jay::NetPacket* resPacket = Jay::NetPacket::Alloc();

	(*resPacket) << (WORD)en_PACKET_CS_GAME_RES_LOGIN;
	(*resPacket) << status;
	(*resPacket) << accountNo;
	SendPacket(sessionID, resPacket);

	Jay::NetPacket::Free(resPacket);
}
void EchoServer::PacketProc_Echo(DWORD64 sessionID, Jay::NetPacket* packet)
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
	SendPacket(sessionID, resPacket);
	
	Jay::NetPacket::Free(resPacket);
}
void EchoServer::UpdateThread()
{
	while (!_stopSignal)
	{
		JOB* job;
		for (int i = 0; i < JOBQUEUE_SIZE; i++)
		{
			while (_jobQ[i].size() > 0)
			{
				_jobQ[i].Dequeue(job);

				switch (job->type)
				{
				case JOB_TYPE_PACKET_PROC:
					PacketProc(job->sessionID, job->packet);
					break;
				default:
					break;
				}

				_jobPool.Free(job);
			}
		}
		_curFPS++;
	}
}
void EchoServer::ManagementThread()
{
	while (!_stopSignal)
	{
		Sleep(1000);
		_oldFPS.exchange(_curFPS.exchange(0));
	}
}

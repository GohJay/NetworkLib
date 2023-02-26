#pragma once
#include "../Network/NetServer.h"
#include "../Network/LockFreeQueue.h"
#include "../Network/LFObjectPool_TLS.h"
#include <thread>
#include <unordered_map>

#define JOBQUEUE_SIZE		10

typedef DWORD64 SESSION_ID;

enum JOB_TYPE
{
	JOB_TYPE_PACKET_PROC = 0
};

struct JOB
{
	JOB_TYPE type;
	DWORD64 sessionID;
	Jay::NetPacket* packet;
};

struct PLAYER
{
	DWORD64 sessionID;
	__int64 accountNo;
	char sessionKey[64];
	bool login;
};

class EchoServer : public Jay::NetServer
{
public:
	EchoServer();
	~EchoServer();
public:
	int GetFPS();
private:
	bool OnConnectionRequest(const wchar_t* ipaddress, int port) override;
	void OnClientJoin(DWORD64 sessionID) override;
	void OnClientLeave(DWORD64 sessionID) override;
	void OnRecv(DWORD64 sessionID, Jay::NetPacket* packet) override;
	void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) override;
private:
	void PacketProc(DWORD64 sessionID, Jay::NetPacket* packet);
	void PacketProc_Login(DWORD64 sessionID, Jay::NetPacket* packet);
	void PacketProc_Echo(DWORD64 sessionID, Jay::NetPacket* packet);
	void UpdateThread();
	void ManagementThread();
private:
	std::unordered_map<SESSION_ID, PLAYER*> _playerMap;
	Jay::LFObjectPool_TLS<PLAYER> _playerPool;
	Jay::LockFreeQueue<JOB*> _jobQ[JOBQUEUE_SIZE];
	Jay::LFObjectPool_TLS<JOB> _jobPool;
	std::atomic<int> _oldFPS;
	std::atomic<int> _curFPS;
	std::thread _updateThread;
	std::thread _managementThread;
	bool _stopSignal;
};

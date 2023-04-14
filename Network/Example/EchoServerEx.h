#pragma once
#include "../Network/NetServerEx.h"
#include "../Network/NetContent.h"
#include <thread>
#include <atomic>

enum CONTENT_ID
{
	CONTENT_ID_AUTH = 0,
	CONTENT_ID_GAME = 1,
};

enum CONTENT_FRAME_INTERVAL
{
	FRAME_INTERVAL_AUTH = 10,
	FRAME_INTERVAL_GAME = 0
};

class EchoServerEx : public Jay::NetServerEx
{
public:
	EchoServerEx();
	~EchoServerEx();
private:
	bool OnConnectionRequest(const wchar_t* ipaddress, int port) override;
	void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) override;
};

class LoginServerEx : public Jay::NetContent
{
public:
	LoginServerEx(EchoServerEx* subject);
	~LoginServerEx();
public:
	int GetFPS();
private:
	void OnStart() override;
	void OnStop() override;
	void OnUpdate() override;
	void OnClientJoin(DWORD64 sessionID) override;
	void OnClientLeave(DWORD64 sessionID) override;
	void OnContentEnter(DWORD64 sessionID, WPARAM wParam, LPARAM lParam) override;
	void OnContentExit(DWORD64 sessionID) override;
	void OnRecv(DWORD64 sessionID, Jay::NetPacket* packet) override;
private:
	void LoginProc(DWORD64 sessionID, Jay::NetPacket* packet);
	void ManagementThread();
private:
	EchoServerEx* _subject;
	std::atomic<int> _oldFPS;
	std::atomic<int> _curFPS;
	std::thread _managementThread;
	bool _stopSignal;
};

class GameServerEx : public Jay::NetContent
{
public:
	GameServerEx(EchoServerEx* subject);
	~GameServerEx();
public:
	int GetFPS();
private:
	void OnStart() override;
	void OnStop() override;
	void OnUpdate() override;
	void OnClientJoin(DWORD64 sessionID) override;
	void OnClientLeave(DWORD64 sessionID) override;
	void OnContentEnter(DWORD64 sessionID, WPARAM wParam, LPARAM lParam) override;
	void OnContentExit(DWORD64 sessionID) override;
	void OnRecv(DWORD64 sessionID, Jay::NetPacket* packet) override;
private:
	void PacketProc_Echo(DWORD64 sessionID, Jay::NetPacket* packet);
	void ManagementThread();
private:
	EchoServerEx* _subject;
	std::atomic<int> _oldFPS;
	std::atomic<int> _curFPS;
	std::thread _managementThread;
	bool _stopSignal;
};

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
	FRAME_INTERVAL_AUTH = 1,
	FRAME_INTERVAL_GAME = 0
};

class EchoServerEx : public Jay::NetServerEx
{
public:
	EchoServerEx();
	~EchoServerEx();
private:
	bool OnConnectionRequest(const wchar_t* ipaddress, int port) override;
	void OnClientJoin(DWORD64 sessionID) override;
	void OnClientLeave(DWORD64 sessionID) override;
	void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) override;
};

class LoginServer : public Jay::NetContent
{
public:
	LoginServer(EchoServerEx* subject);
	~LoginServer();
public:
	int GetFPS();
private:
	void OnUpdate() override;
	void OnContentJoin(DWORD64 sessionID) override;
	void OnContentLeave(DWORD64 sessionID) override;
	void OnRecv(DWORD64 sessionID, Jay::NetPacket* packet) override;
private:
	void ManagementThread();
private:
	EchoServerEx* _subject;
	std::atomic<int> _oldFPS;
	std::atomic<int> _curFPS;
	std::thread _managementThread;
	bool _stopSignal;
};

class GameServer : public Jay::NetContent
{
public:
	GameServer(EchoServerEx* subject);
	~GameServer();
public:
	int GetFPS();
private:
	void OnUpdate() override;
	void OnContentJoin(DWORD64 sessionID) override;
	void OnContentLeave(DWORD64 sessionID) override;
	void OnRecv(DWORD64 sessionID, Jay::NetPacket* packet) override;
private:
	void ManagementThread();
private:
	EchoServerEx* _subject;
	std::atomic<int> _oldFPS;
	std::atomic<int> _curFPS;
	std::thread _managementThread;
	bool _stopSignal;
};

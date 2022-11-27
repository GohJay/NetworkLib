#pragma once
#include "../Network/LanServer.h"

class EchoServer : public Jay::LanServer
{
public:
	EchoServer();
	~EchoServer();
private:
	bool OnConnectionRequest(const wchar_t* ipaddress, int port);
	void OnClientJoin(DWORD64 sessionID);
	void OnClientLeave(DWORD64 sessionID);
	void OnRecv(DWORD64 sessionID, Jay::SerializationBuffer* packet);
	void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam);
};

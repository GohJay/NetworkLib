#ifndef __LANSERVER__H_
#define __LANSERVER__H_
#include "Base.h"
#include "define.h"
#include "SerializationBuffer.h"
#include <stack>

JAYNAMESPACE
class LanServer
{
	/**
	* @file		LanServer.h
	* @brief	Network LanServer Class
	* @details	내부 네트워크의 클라이언트와 통신을 목적으로한 IOCP 서버 클래스
	* @author   고재현
	* @date		2022-11-26
	* @version  1.0.1
	**/
public:
	LanServer();
	~LanServer();
public:
	bool Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, bool nagle = true);
	void Stop();
	bool Disconnect(DWORD64 sessionID);
	bool SendPacket(DWORD64 sessionID, SerializationBuffer* packet);
	int GetSessionCount();
	int GetAcceptTPS();
	int GetRecvTPS();
	int GetSendTPS();
protected:
	virtual bool OnConnectionRequest(const wchar_t* ipaddress, int port) = 0;
	virtual void OnClientJoin(DWORD64 sessionID) = 0;
	virtual void OnClientLeave(DWORD64 sessionID) = 0;
	virtual void OnRecv(DWORD64 sessionID, SerializationBuffer* packet) = 0;
	virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
private:
	bool Listen(const wchar_t* ipaddress, int port, bool nagle);
	bool Initial();
	void Release();
	SESSION* CreateSession(SOCKET socket, const wchar_t* ipaddress, int port);
	void DisconnectSession(SESSION* session);
	void RecvPost(SESSION* session);
	void SendPost(SESSION* session);
	void CompleteRecvPacket(SESSION* session);
	void CompleteSendPacket(SESSION* session);
	void SendUnicast(SESSION* session, SerializationBuffer* packet);
	void CleanupSendBuffer(SESSION* session);
	SESSION* AcquireSessionLock(DWORD64 sessionID);
	void ReleaseSessionLock(SESSION* session);
	void MessageProc(UINT message, WPARAM wParam, LPARAM lParam);
private:
	unsigned int AcceptThread();
	unsigned int WorkerThread();
	unsigned int MonitoringThread();
	static unsigned int WINAPI WrapAcceptThread(LPVOID lpParam);
	static unsigned int WINAPI WrapWorkerThread(LPVOID lpParam);
	static unsigned int WINAPI WrapMonitoringThread(LPVOID lpParam);
private:
	SESSION* _sessionArray;
	WORD _sessionMax;
	WORD _sessionCnt;
	DWORD64 _sessionKey;
	std::stack<WORD> _indexStack;
	SRWLOCK _indexLock;
	SOCKET _listenSocket;
	HANDLE _hCompletionPort;
	int _workerCreateCnt;
	int _workerRunningCnt;
	HANDLE* _hWorkerThread;
	HANDLE _hAcceptThread;
	HANDLE _hMonitoringThread;
	HANDLE _hExitThreadEvent;
	MONITORING _monitoring;
};
JAYNAMESPACEEND

#endif

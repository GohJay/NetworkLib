#ifndef __LANSERVER__H_
#define __LANSERVER__H_
#include "Base.h"
#include "Protocol.h"
#include "SerializationBuffer.h"
#include "RingBuffer.h"
#include <stack>

JAYNAMESPACE
/**
* @file		LanServer.h
* @brief	Network LanServer Class
* @details	내부 네트워크의 클라이언트와 통신을 목적으로한 IOCP 서버 클래스
* @author   고재현
* @date		2022-11-22
* @version  1.0.0
**/
class LanServer
{
private:
	struct SESSION
	{
		SESSION() : sessionID(-1)
		{
			InitializeSRWLock(&lock);
		}
		OVERLAPPED recvOverlapped;
		OVERLAPPED sendOverlapped;
		SRWLOCK lock;
		DWORD64 sessionID;
		SOCKET socket;
		wchar_t ip[16];
		int port;
		LONG ioCount;
		RingBuffer recvQ;
		RingBuffer sendQ;
		BOOL sendFlag;
	};
	struct TPS
	{
		LONG accept;
		LONG recv;
		LONG send;
	};
	struct MONITORING
	{
		TPS oldTPS;
		TPS curTPS;
	};
public:
	LanServer();
	~LanServer();
public:
	bool Start(wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, bool nagle = true);
	void Stop();
	bool Disconnect(DWORD64 sessionID);
	bool SendPacket(DWORD64 sessionID, SerializationBuffer* packet);
	int GetSessionCount();
	int GetAcceptTPS();
	int GetRecvTPS();
	int GetSendTPS();
protected:
	virtual bool OnConnectionRequest(wchar_t* ipaddress, int port) = 0;
	virtual void OnClientJoin(DWORD64 sessionID) = 0;
	virtual void OnClientLeave(DWORD64 sessionID) = 0;
	virtual void OnRecv(DWORD64 sessionID, SerializationBuffer* packet) = 0;
	virtual void OnError(int error, const wchar_t* fmt, ...) = 0;
private:
	SESSION* CreateSession(SOCKET socket, wchar_t* ipaddress, int port);
	void DisconnectSession(SESSION* session);
	void RecvPost(SESSION* session);
	void SendPost(SESSION* session);
	void CompleteRecvPacket(SESSION* session);
	void SendUnicast(SESSION* session, LAN_PACKET_HEADER* header, SerializationBuffer* packet);
	void MakeHeader(LAN_PACKET_HEADER* header, SerializationBuffer* packet);
	SESSION* AcquireSessionLock(DWORD64 sessionID);
	void ReleaseSessionLock(SESSION* session);
private:
	bool Listen(wchar_t* ipaddress, int port, bool nagle);
	bool Initial();
	void Release();
	void MessageProc(UINT message, WPARAM wParam, LPARAM lParam);
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

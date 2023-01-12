#ifndef __LANSERVER__H_
#define __LANSERVER__H_
#include "Base.h"
#include "define.h"
#include "NetPacketPtr.h"
#include "LockFreeStack.h"

JAYNAMESPACE
class LanServer
{
	/**
	* @file		LanServer.h
	* @brief	Network LanServer Class
	* @details	���� ��Ʈ��ũ�� Ŭ���̾�Ʈ�� ����� ���������� IOCP ���� Ŭ����
	* @author   ������
	* @date		2023-01-12
	* @version  1.0.2
	**/
public:
	LanServer();
	virtual ~LanServer();
public:
	bool Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, bool nagle = true);
	void Stop();
	bool Disconnect(DWORD64 sessionID);
	bool SendPacket(DWORD64 sessionID, NetPacketPtr packet);
	int GetSessionCount();
	int GetUsePacketCount();
	int GetAcceptTPS();
	int GetRecvTPS();
	int GetSendTPS();
protected:
	virtual bool OnConnectionRequest(const wchar_t* ipaddress, int port) = 0;
	virtual void OnClientJoin(DWORD64 sessionID) = 0;
	virtual void OnClientLeave(DWORD64 sessionID) = 0;
	virtual void OnRecv(DWORD64 sessionID, NetPacketPtr packet) = 0;
	virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
private:
	bool Listen(const wchar_t* ipaddress, int port, bool nagle);
	bool Initial();
	void Release();
	SESSION* CreateSession(SOCKET socket, const wchar_t* ipaddress, int port);
	void ReleaseSession(SESSION* session);
	void DisconnectSession(SESSION* session);
	SESSION* DuplicateSession(DWORD64 sessionID);
	void CloseSession(SESSION* session);
	void RecvPost(SESSION* session);
	void SendPost(SESSION* session);
	void CompleteRecvPacket(SESSION* session);
	void CompleteSendPacket(SESSION* session);
	void TrySendPacket(SESSION* session, NetPacket* packet);
	void ClearSendPacket(SESSION* session);
	void MessagePost(DWORD message, LPVOID lpParam);
	void MessageProc(DWORD message, LPVOID lpParam);
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
	LockFreeStack<WORD> _indexStack;
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

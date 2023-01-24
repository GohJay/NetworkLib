#ifndef __NETSERVER__H_
#define __NETSERVER__H_
#include "Base.h"
#include "Define.h"
#include "NetPacket.h"
#include "LockFreeStack.h"

namespace Jay
{
	class NetServer
	{
		/**
		* @file		NetServer.h
		* @brief	Network NetServer Class
		* @details	외부 네트워크의 클라이언트와 통신을 목적으로한 IOCP 서버 클래스
		* @author   고재현
		* @date		2023-01-22
		* @version  1.0.1
		**/
	public:
		NetServer();
		virtual ~NetServer();
	public:
		bool Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, BYTE packetCode, BYTE packetKey, int timeoutSec = 0, bool nagle = true);
		void Stop();
		bool Disconnect(DWORD64 sessionID);
		bool SendPacket(DWORD64 sessionID, NetPacket* packet);
		int GetSessionCount();
		int GetUsePacketCount();
		int GetTotalAcceptCount();
		int GetAcceptTPS();
		int GetRecvTPS();
		int GetSendTPS();
	protected:
		virtual bool OnConnectionRequest(const wchar_t* ipaddress, int port) = 0;
		virtual void OnClientJoin(DWORD64 sessionID) = 0;
		virtual void OnClientLeave(DWORD64 sessionID) = 0;
		virtual void OnRecv(DWORD64 sessionID, NetPacket* packet) = 0;
		virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
	private:
		SESSION* CreateSession(SOCKET socket, const wchar_t* ipaddress, int port);
		void ReleaseSession(SESSION* session);
		void DisconnectSession(SESSION* session);
		SESSION* DuplicateSession(DWORD64 sessionID);
		void CloseSession(SESSION* session);
		void RecvPost(SESSION* session);
		void SendPost(SESSION* session);
		void RecvRoutine(SESSION* session, DWORD cbTransferred);
		void SendRoutine(SESSION* session, DWORD cbTransferred);
		void CompleteRecvPacket(SESSION* session);
		void CompleteSendPacket(SESSION* session);
		void TrySendPacket(SESSION* session, NetPacket* packet);
		void ClearSendPacket(SESSION* session);
		void QueueUserMessage(DWORD message, LPVOID lpParam);
		void UserMessageProc(DWORD message, LPVOID lpParam);
		void TimeoutProc();
		void UpdateTPS();
	private:
		bool Listen(const wchar_t* ipaddress, int port, bool nagle);
		bool Initial();
		void Release();
		unsigned int AcceptThread();
		unsigned int WorkerThread();
		unsigned int ManagementThread();
		static unsigned int WINAPI WrapAcceptThread(LPVOID lpParam);
		static unsigned int WINAPI WrapWorkerThread(LPVOID lpParam);
		static unsigned int WINAPI WrapManagementThread(LPVOID lpParam);
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
		HANDLE _hManagementThread;
		HANDLE _hExitThreadEvent;
		DWORD _lastTimeoutProc;
		int _timeoutSec;
		int _totalAcceptCnt;
		TPS _curTPS;
		TPS _oldTPS;
		BYTE _packetCode;
		BYTE _packetKey;
	};
}

#endif

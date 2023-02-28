#ifndef __NETSERVEREX__H_
#define __NETSERVEREX__H_
#include "Base.h"
#include "Session.h"
#include "Content.h"
#include "NetContent.h"
#include "NetPacket.h"
#include "LFStack.h"

namespace Jay
{
	class NetServerEx
	{
		/**
		* @file		NetServerEx.h
		* @brief	Network NetServer Extension Class
		* @details	외부 네트워크의 게임 클라이언트와 통신을 목적으로한 IOCP 서버 클래스
		* @author	고재현
		* @date		2023-02-26
		* @version	1.0.2
		**/
	public:
		NetServerEx();
		virtual ~NetServerEx();
	public:
		void AttachContent(NetContent* handler, WORD contentID, WORD frameInterval, bool default = false);
		bool SetFrameInterval(WORD contentID, WORD frameInterval);
		bool Start(const wchar_t* ipaddress, int port, int workerCreateCount, int workerRunningCount, WORD sessionMax, BYTE packetCode, BYTE packetKey, int timeoutSec = 0, bool nagle = true);
		void Stop();
		bool Disconnect(DWORD64 sessionID);
		bool SendPacket(DWORD64 sessionID, NetPacket* packet);
		bool MoveContent(DWORD64 sessionID, WORD contentID, WPARAM wParam, LPARAM lParam);
		int GetSessionCount();
		int GetUsePacketPool();
		int GetCapacityPacketPool();
		int GetAcceptTPS();
		int GetRecvTPS();
		int GetSendTPS();
		__int64 GetTotalAcceptCount();
	protected:
		virtual bool OnConnectionRequest(const wchar_t* ipaddress, int port) = 0;
		virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
	private:
		SESSION* CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr);
		void ReleaseSession(SESSION* session);
		void DisconnectSession(SESSION* session);
		SESSION* DuplicateSession(DWORD64 sessionID);
		void IncrementIOCount(SESSION* session);
		void CloseSession(SESSION* session);
		void RecvPost(SESSION* session);
		void SendPost(SESSION* session);
		void RecvRoutine(SESSION* session, DWORD cbTransferred);
		void SendRoutine(SESSION* session, DWORD cbTransferred);
		int CompleteRecvPacket(SESSION* session);
		void CompleteSendPacket(SESSION* session);
		void TrySendPacket(SESSION* session, NetPacket* packet);
		void ClearSendPacket(SESSION* session);
		void ClearSessionJob(SESSION* session);
		void QueueUserMessage(DWORD message, LPVOID lpParam);
		void UserMessageProc(DWORD message, LPVOID lpParam);
		void TimeoutProc();
		void UpdateTPS();
		void TryMoveContent(SESSION* session, WORD contentID, WPARAM wParam, LPARAM lParam);
		CONTENT* FindContent(WORD contentID);
		CONTENT* GetCurrentContent();
		bool SessionJobProc(SESSION* session, NetContent* handler);
		bool ContentJobProc(CONTENT* content);
		void NotifyContent(CONTENT* content);
	private:
		bool Listen(const wchar_t* ipaddress, int port, bool nagle);
		bool Initial();
		void Release();
		unsigned int AcceptThread();
		unsigned int WorkerThread();
		unsigned int ManagementThread();
		unsigned int ContentThread();
		static unsigned int WINAPI WrapAcceptThread(LPVOID lpParam);
		static unsigned int WINAPI WrapWorkerThread(LPVOID lpParam);
		static unsigned int WINAPI WrapManagementThread(LPVOID lpParam);
		static unsigned int WINAPI WrapContentThread(LPVOID lpParam);
	private:
		SESSION* _sessionArray;
		WORD _sessionMax;
		WORD _sessionCount;
		DWORD64 _sessionKey;
		LFStack<WORD> _indexStack;
		int _workerCreateCount;
		int _workerRunningCount;
		int _timeoutSec;
		BYTE _packetCode;
		BYTE _packetKey;
		SOCKET _listenSocket;
		HANDLE _hCompletionPort;
		HANDLE* _hWorkerThread;
		HANDLE _hAcceptThread;
		HANDLE _hManagementThread;
		HANDLE* _hContentThread;
		CONTENT _contentArray[MAX_CONTENT];
		WORD _defaultContentIndex;
		WORD _contentCount;
		DWORD _lastTimeoutProc;
		MONITORING _monitoring;
		volatile bool _stopSignal;
		LFObjectPool_TLS<SESSION_JOB> _sessionJobPool;
		LFObjectPool_TLS<CONTENT_JOB> _contentJobPool;
	};
}

#endif

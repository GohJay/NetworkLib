#ifndef __NETSERVEREX__H_
#define __NETSERVEREX__H_
#include "Base.h"
#include "Define.h"
#include "NetContent.h"
#include "NetPacket.h"
#include "LockFreeStack.h"
#include "List.h"

namespace Jay
{
	class NetServerEx
	{
		/**
		* @file		NetServerEx.h
		* @brief	Network NetServer Extension Class
		* @details	외부 네트워크의 게임 클라이언트와 통신을 목적으로한 IOCP 서버 클래스
		* @author	고재현
		* @date		2023-02-20
		* @version	1.0.0
		**/
	private:
		enum CONTENT_JOB_TYPE
		{
			JOB_TYPE_CONTENT_JOIN = 0
		};
		struct CONTENT_JOB
		{
			CONTENT_JOB_TYPE type;
			DWORD64 sessionID;
		};
		struct CONTENT_INFO
		{
			NetContent* content;
			WORD contentID;
			WORD frameInterval;
			DWORD threadID;
			List<DWORD64> sessionIDList;
			LockFreeQueue<CONTENT_JOB*> jobQ;
		};
	public:
		NetServerEx();
		virtual ~NetServerEx();
	public:
		void AttachContent(NetContent* content, WORD contentID, WORD frameInterval, bool default = false);
		bool ChangeFrameInterval(WORD contentID, WORD frameInterval);
		bool Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, BYTE packetCode, BYTE packetKey, int timeoutSec = 0, bool nagle = true);
		void Stop();
		bool Disconnect(DWORD64 sessionID);
		bool SendPacket(DWORD64 sessionID, NetPacket* packet);
		bool MoveContent(DWORD64 sessionID, WORD contentID);
		int GetSessionCount();
		int GetUsePacketCount();
		int GetAcceptTPS();
		int GetRecvTPS();
		int GetSendTPS();
		__int64 GetTotalAcceptCount();
	protected:
		virtual bool OnConnectionRequest(const wchar_t* ipaddress, int port) = 0;
		virtual void OnClientJoin(DWORD64 sessionID) = 0;
		virtual void OnClientLeave(DWORD64 sessionID) = 0;
		virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
	private:
		SESSION* CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr);
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
		void ClearSessionJob(SESSION* session);
		void QueueUserMessage(DWORD message, LPVOID lpParam);
		void UserMessageProc(DWORD message, LPVOID lpParam);
		void TimeoutProc();
		void UpdateTPS();
		void TryMoveContent(SESSION* session, WORD contentID);
		CONTENT_INFO* FindContentInfo(WORD contentID);
		CONTENT_INFO* GetCurrentContentInfo();
		bool ContentJobProc();
		bool SessionJobProc(SESSION* session, NetContent* content);
		void NotifyContent();
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
		HANDLE* _hContentThread;
		CONTENT_INFO _contentArray[MAX_CONTENT];
		WORD _defaultContentIndex;
		WORD _contentCnt;
		DWORD _tlsContent;
		DWORD _lastTimeoutProc;
		int _timeoutSec;
		MONITORING _monitoring;
		BYTE _packetCode;
		BYTE _packetKey;
		ObjectPool_TLS<SESSION_JOB> _sessionJobPool;
		ObjectPool_TLS<CONTENT_JOB> _contentJobPool;
	};
}

#endif

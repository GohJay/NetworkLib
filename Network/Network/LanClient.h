#ifndef __LANCLIENT__H_
#define __LANCLIENT__H_
#include "Base.h"
#include "Session.h"
#include "NetPacket.h"

namespace Jay
{
	class LanClient
	{
		/**
		* @file		LanClient.h
		* @brief	Network LanClient Class
		* @details	외부 네트워크의 서버와 통신을 목적으로한 IOCP 클라이언트 클래스
		* @author	고재현
		* @date		2023-02-27
		* @version	1.0.1
		**/
	public:
		LanClient();
		virtual ~LanClient();
	public:
		bool Connect(const wchar_t* ipaddress, int port, bool nagle = true);
		bool Disconnect();
		bool SendPacket(NetPacket* packet);
	protected:
		virtual void OnEnterJoinServer() = 0;
		virtual void OnLeaveServer() = 0;
		virtual void OnRecv(NetPacket* packet) = 0;
		virtual void OnError(int errcode, const wchar_t* funcname, int linenum, WPARAM wParam, LPARAM lParam) = 0;
	private:
		SESSION* CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr);
		void ReleaseSession(SESSION* session);
		void DisconnectSession(SESSION* session);
		SESSION* DuplicateSession();
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
		void QueueUserMessage(DWORD message, LPVOID lpParam);
		void UserMessageProc(DWORD message, LPVOID lpParam);
	private:
		unsigned int WorkerThread();
		static unsigned int WINAPI WrapWorkerThread(LPVOID lpParam);
	private:
		SESSION _session;
		HANDLE _hCompletionPort;
		HANDLE _hWorkerThread;
	};
}

#endif

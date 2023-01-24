#ifndef __LANCLIENT__H_
#define __LANCLIENT__H_
#include "Base.h"
#include "define.h"
#include "NetPacket.h"

namespace Jay
{
	/**
	* @file		LanClient.h
	* @brief	Network LanClient Class
	* @details	���� ��Ʈ��ũ�� ������ ����� ���������� IOCP Ŭ���̾�Ʈ Ŭ����
	* @author   ������
	* @date		2023-01-24
	* @version  1.0.0
	**/
	class LanClient
	{
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
		SESSION* CreateSession(SOCKET socket, const wchar_t* ipaddress, int port);
		void ReleaseSession(SESSION* session);
		SESSION* DuplicateSession();
		void DisconnectSession(SESSION* session);
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
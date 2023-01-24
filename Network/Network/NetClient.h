#ifndef __NETCLIENT__H_
#define __NETCLIENT__H_
#include "Base.h"
#include "Define.h"
#include "NetPacket.h"

namespace Jay
{
	/**
	* @file		NetClient.h
	* @brief	Network NetClient Class
	* @details	�ܺ� ��Ʈ��ũ�� ������ ����� ���������� IOCP Ŭ���̾�Ʈ Ŭ����
	* @author   ������
	* @date		2023-01-24
	* @version  1.0.0
	**/
	class NetClient
	{
	public:
		NetClient();
		virtual ~NetClient();
	public:
		bool Connect(const wchar_t* ipaddress, int port, BYTE packetCode, BYTE packetKey, bool nagle = true);
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
		BYTE _packetCode;
		BYTE _packetKey;
	};
}

#endif
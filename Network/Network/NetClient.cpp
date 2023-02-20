#include "NetClient.h"
#include "Error.h"
#include "Protocol.h"
#include "User.h"
#include "Logger.h"
#include "Enviroment.h"
#include "NetException.h"
#include <process.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

using namespace Jay;

NetClient::NetClient()
{
	WSADATA ws;
	int status = WSAStartup(MAKEWORD(2, 2), &ws);
	if (status != 0)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, status);
		return;
	}

	//--------------------------------------------------------------------
	// Initial
	//--------------------------------------------------------------------
	_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (_hCompletionPort == NULL)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, GetLastError());
		return;
	}

	//--------------------------------------------------------------------
	// WorkerThread Begin
	//--------------------------------------------------------------------
	_hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WrapWorkerThread, this, 0, NULL);
}
NetClient::~NetClient()
{
	//--------------------------------------------------------------------
	// �����ִ� ���� ����
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
		DisconnectSession(&_session);

	//--------------------------------------------------------------------
	// ���� ���� ���
	//--------------------------------------------------------------------
	while (_session.releaseFlag != TRUE)
		Sleep(500);

	//--------------------------------------------------------------------
	// WorkerThread End
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);
	WaitForSingleObject(_hWorkerThread, INFINITE);

	//--------------------------------------------------------------------
	// Release
	//--------------------------------------------------------------------
	CloseHandle(_hCompletionPort);
	CloseHandle(_hWorkerThread);

	WSACleanup();
}
bool NetClient::Connect(const wchar_t* ipaddress, int port, BYTE packetCode, BYTE packetKey, bool nagle)
{
	//--------------------------------------------------------------------
	// �̹� ����� ������ �ִ��� Ȯ��
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
	{
		OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, NULL);
		return false;
	}

	SOCKADDR_IN svrAddr;
	ZeroMemory(&svrAddr, sizeof(svrAddr));
	svrAddr.sin_family = AF_INET;
	svrAddr.sin_port = htons(port);
	InetPton(AF_INET, ipaddress, &svrAddr.sin_addr);

	//--------------------------------------------------------------------
	// ���ῡ ����� ���� �Ҵ�
	//--------------------------------------------------------------------
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		return false;
	}

	int option;
	int ret;
	if (!nagle)
	{
		option = TRUE;
		ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&option, sizeof(option));
		if (ret == SOCKET_ERROR)
		{
			OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
			closesocket(sock);
			return false;
		}
	}

	//--------------------------------------------------------------------
	// ������ ����
	//--------------------------------------------------------------------
	ret = connect(sock, (SOCKADDR*)&svrAddr, sizeof(svrAddr));
	if (ret == SOCKET_ERROR)
	{
		OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		closesocket(sock);
		return false;
	}

	_packetCode = packetCode;
	_packetKey = packetKey;

	//--------------------------------------------------------------------
	// ���� �Ҵ�
	//--------------------------------------------------------------------
	SESSION* session = CreateSession(sock, ipaddress, port);

	//--------------------------------------------------------------------
	// �Ҵ�� ������ IOCP�� ���
	//--------------------------------------------------------------------
	CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)&session, NULL);

	//--------------------------------------------------------------------
	// ���� ������ ������ �ο� �˸��� ���� ���
	//--------------------------------------------------------------------
	OnEnterJoinServer();
	RecvPost(session);

	//--------------------------------------------------------------------
	// �ش� �����忡�� �����ϴ� ���� ��ȯ
	//--------------------------------------------------------------------
	CloseSession(session);
	return true;
}
bool NetClient::Disconnect()
{
	SESSION* session = DuplicateSession();
	if (session != nullptr)
	{
		DisconnectSession(session);
		CloseHandle(session);
		return true;
	}
	return false;
}
bool NetClient::SendPacket(NetPacket* packet)
{
	SESSION* session = DuplicateSession();
	if (session != nullptr)
	{
		TrySendPacket(session, packet);
		CloseSession(session);
		return true;
	}
	return false;
}
SESSION* NetClient::CreateSession(SOCKET socket, const wchar_t* ipaddress, int port)
{
	InterlockedIncrement(&_session.ioCount);
	_session.socket = socket;
	wcscpy_s(_session.ip, ipaddress);
	_session.port = port;
	_session.recvQ.ClearBuffer();
	_session.sendBufCount = 0;
	_session.sendFlag = FALSE;
	_session.disconnectFlag = FALSE;
	_session.releaseFlag = FALSE;
	return &_session;
}
void NetClient::ReleaseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ������ IOCount, releaseFlag �� ��� 0 ���� Ȯ��
	//--------------------------------------------------------------------
	if (InterlockedCompareExchange64(&session->release, TRUE, FALSE) != FALSE)
		return;

	//--------------------------------------------------------------------
	// ���� �ڿ� ����
	//--------------------------------------------------------------------
	ClearSendPacket(session);
	closesocket(session->socket);

	//--------------------------------------------------------------------
	// ������ �ο� �˸� ��û
	//--------------------------------------------------------------------
	QueueUserMessage(UM_ALERT_SERVER_LEAVE, NULL);
}
SESSION* NetClient::DuplicateSession()
{
	InterlockedIncrement(&_session.ioCount);

	//--------------------------------------------------------------------
	// ������� �������� Ȯ��
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
	{
		// ã�� ���� ȹ��
		return &_session;
	}

	if (InterlockedDecrement(&_session.ioCount) == 0)
		ReleaseSession(&_session);

	return nullptr;
}
void NetClient::DisconnectSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���Ͽ� ���� ��û�Ǿ��ִ� ��� IO �� �ߴ�
	//--------------------------------------------------------------------
	if (InterlockedExchange((LONG*)&session->disconnectFlag, TRUE) == FALSE)
		CancelIoEx((HANDLE)session->socket, NULL);
}
void NetClient::CloseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���� ���� ��ȯ
	//--------------------------------------------------------------------
	if (InterlockedDecrement(&session->ioCount) == 0)
		ReleaseSession(session);
}
void NetClient::RecvPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// Disconnect Flag �� �������� ��� return
	//--------------------------------------------------------------------
	if (session->disconnectFlag == TRUE)
		return;

	//--------------------------------------------------------------------
	// ���ſ� �������� ������ ���ϱ�	
	//--------------------------------------------------------------------
	int directSize = session->recvQ.DirectEnqueueSize();
	if (directSize <= 0)
	{
		// ���������� ���� ���ٴ� ���� �����ۿ� ��� �޽����� ������ �ο��� �Ľ� �� �� ���� ������ �ִ� ��. ������ ���´�.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		DisconnectSession(session);
		return;
	}

	//--------------------------------------------------------------------
	// WSARecv �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(session->recvOverlapped, sizeof(OVERLAPPED));
	WSABUF wsaRecvBuf[2];
	wsaRecvBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaRecvBuf[0].len = directSize;
	wsaRecvBuf[1].len = session->recvQ.GetFreeSize() - directSize;
	wsaRecvBuf[1].buf = session->recvQ.GetBufferPtr();

	//--------------------------------------------------------------------
	// WSARecv ó��
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	DWORD flag = 0;
	int ret = WSARecv(session->socket, wsaRecvBuf, 2, NULL, &flag, session->recvOverlapped, NULL);
	if (ret == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			switch (err)
			{
			case WSAECONNABORTED:
			case WSAECONNRESET:
				break;
			default:
				OnError(NET_ERROR_SOCKET_FAILED, __FUNCTIONW__, __LINE__, session->sessionID, err);
				break;
			}

			// ioCount�� 0�̶�� ���� ����
			if (InterlockedDecrement(&session->ioCount) == 0)
				ReleaseSession(session);
			return;
		}

		//--------------------------------------------------------------------
		// WSARecv �۾� �ߴ� ���� �Ǵ�
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			CancelIoEx((HANDLE)session->socket, session->recvOverlapped);
	}
}
void NetClient::SendPost(SESSION* session)
{
	for (;;)
	{
		//--------------------------------------------------------------------
		// Disconnect Flag �� �������� ��� return
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			return;

		//--------------------------------------------------------------------
		// �̹� ���� ���� Send ���� ������ return
		//--------------------------------------------------------------------
		if (InterlockedExchange((LONG*)&session->sendFlag, TRUE) == TRUE)
			return;

		if (session->sendQ.size() > 0)
			break;

		//--------------------------------------------------------------------
		// ������ �����Ͱ� �ִ��� ���� Ȯ�� �� ������ return
		//--------------------------------------------------------------------
		InterlockedExchange((LONG*)&session->sendFlag, FALSE);
		if (session->sendQ.size() <= 0)
			return;
	}

	//--------------------------------------------------------------------
	// WSASend �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(session->sendOverlapped, sizeof(OVERLAPPED));
	WSABUF wsaBuf[MAX_SENDBUF];

	//--------------------------------------------------------------------
	// �۽��� ����ȭ ���� ������ Dequeue
	//--------------------------------------------------------------------
	NetPacket* packet;
	int count;
	for (count = 0; count < MAX_SENDBUF; count++)
	{
		if (!session->sendQ.Dequeue(session->sendBuf[count]))
			break;

		packet = session->sendBuf[count];
		wsaBuf[count].len = packet->GetPacketSize();
		wsaBuf[count].buf = packet->GetHeaderPtr();
	}
	session->sendBufCount = count;

	//--------------------------------------------------------------------
	// WSASend ó��
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	int ret = WSASend(session->socket, wsaBuf, count, NULL, 0, session->sendOverlapped, NULL);
	if (ret == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			switch (err)
			{
			case WSAECONNABORTED:
			case WSAECONNRESET:
				break;
			default:
				OnError(NET_ERROR_SOCKET_FAILED, __FUNCTIONW__, __LINE__, session->sessionID, err);
				break;
			}

			// ioCount�� 0�̶�� ���� ����
			if (InterlockedDecrement(&session->ioCount) == 0)
				ReleaseSession(session);
			return;
		}

		//--------------------------------------------------------------------
		// WSASend �۾� �ߴ� ���� �Ǵ�
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			CancelIoEx((HANDLE)session->socket, session->sendOverlapped);
	}
}
void NetClient::RecvRoutine(SESSION* session, DWORD cbTransferred)
{
	session->recvQ.MoveRear(cbTransferred);
	CompleteRecvPacket(session);
	RecvPost(session);
}
void NetClient::SendRoutine(SESSION* session, DWORD cbTransferred)
{
	CompleteSendPacket(session);
	InterlockedExchange((LONG*)&session->sendFlag, FALSE);
	if (session->sendQ.size() > 0)
		SendPost(session);
}
void NetClient::CompleteRecvPacket(SESSION* session)
{
	NET_PACKET_HEADER header;
	for (;;)
	{
		//--------------------------------------------------------------------
		// ���ſ� �������� ����� Header ũ�⺸�� ū�� Ȯ��
		//--------------------------------------------------------------------
		int size = session->recvQ.GetUseSize();
		if (size <= sizeof(header))
			break;

		//--------------------------------------------------------------------
		// ���ſ� �����ۿ��� Header �� Peek �Ͽ� Ȯ��
		//--------------------------------------------------------------------
		int ret = session->recvQ.Peek((char*)&header, sizeof(header));
		if (ret != sizeof(header))
		{
			Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, peek size: %d, ret size: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE, sizeof(header), ret);
			OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			break;
		}

		//--------------------------------------------------------------------
		// Payload ũ�Ⱑ �����ص� �ִ� ũ�⸦ �ʰ��ϴ��� Ȯ��
		//--------------------------------------------------------------------
		if (header.len > MAX_PAYLOAD)
		{
			DisconnectSession(session);
			break;
		}

		//--------------------------------------------------------------------
		// ���ſ� �������� ����� Header + Payload ũ�� ��ŭ �ִ��� Ȯ��
		//--------------------------------------------------------------------
		if (session->recvQ.GetUseSize() < sizeof(header) + header.len)
			break;

		session->recvQ.MoveFront(sizeof(header));

		//--------------------------------------------------------------------
		// ����ȭ ���ۿ� Header ���
		//--------------------------------------------------------------------
		NetPacket* packet = NetPacket::Alloc();
		packet->PutHeader((char*)&header, sizeof(header));

		//--------------------------------------------------------------------
		// ����ȭ ���ۿ� Payload ���
		//--------------------------------------------------------------------
		ret = session->recvQ.Dequeue(packet->GetRearBufferPtr(), header.len);
		if (ret != header.len)
		{
			Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, dequeue size: %d, ret size: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE, header.len, ret);
			OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			NetPacket::Free(packet);
			break;
		}
		packet->MoveRear(ret);

		//--------------------------------------------------------------------
		// ����ȭ ���� ���ڵ� ó��
		//--------------------------------------------------------------------
		if (!packet->Decode(_packetCode, _packetKey))
		{
			DisconnectSession(session);
			NetPacket::Free(packet);
			break;
		}

		try
		{
			//--------------------------------------------------------------------
			// ������ �ο� ����ȭ ���� ����
			//--------------------------------------------------------------------
			OnRecv(packet);
		}
		catch (NetException& ex)
		{
			OnError(ex.GetLastError(), __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			NetPacket::Free(packet);
			return;
		}

		NetPacket::Free(packet);
	}
}
void NetClient::CompleteSendPacket(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���� �Ϸ��� ����ȭ ���� ����
	//--------------------------------------------------------------------
	NetPacket* packet;
	int count;
	for (count = 0; count < session->sendBufCount; count++)
	{
		packet = session->sendBuf[count];
		NetPacket::Free(packet);
	}

	session->sendBufCount = 0;
}
void NetClient::TrySendPacket(SESSION* session, NetPacket* packet)
{
	int size = session->sendQ.size();
	if (size > MAX_SENDBUF)
	{
		// ���������� �����Ͽ� �޽����� ���� �� ���ٴ� ���� ������ ó������ �� �ִ� �Ѱ踦 �����ٴ� ��. ������ ���´�.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, size);
		DisconnectSession(session);
		return;
	}

	//--------------------------------------------------------------------
	// ����ȭ ���� ���ڵ� ó��
	//--------------------------------------------------------------------
	packet->Encode(_packetCode, _packetKey);

	//--------------------------------------------------------------------
	// �۽ſ� ť�� ����ȭ ���� ���
	//--------------------------------------------------------------------
	packet->IncrementRefCount();
	session->sendQ.Enqueue(packet);

	//--------------------------------------------------------------------
	// �۽� ��û
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	QueueUserMessage(UM_POST_SEND_PACKET, (LPVOID)session);
}
void NetClient::ClearSendPacket(SESSION* session)
{
	NetPacket* packet;
	int count;
	for (count = 0; count < session->sendBufCount; count++)
	{
		//--------------------------------------------------------------------
		// ���� ������̴� ����ȭ ���� ����
		//--------------------------------------------------------------------
		packet = session->sendBuf[count];
		NetPacket::Free(packet);
	}
	session->sendBufCount = 0;

	while (session->sendQ.size() > 0)
	{
		//--------------------------------------------------------------------
		// �۽ſ� ť�� �����ִ� ����ȭ ���� ����
		//--------------------------------------------------------------------
		session->sendQ.Dequeue(packet);
		NetPacket::Free(packet);
	}
}
void NetClient::QueueUserMessage(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// WorkerThread ���� Job ��û
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, message, (ULONG_PTR)lpParam, NULL);
}
void NetClient::UserMessageProc(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// �ٸ� ������κ��� ��û���� Job ó��
	//--------------------------------------------------------------------
	switch (message)
	{
	case UM_POST_SEND_PACKET:
		{
			SESSION* session = (SESSION*)lpParam;
			SendPost(session);
			CloseSession(session);
		}
		break;
	case UM_ALERT_SERVER_LEAVE:
		{
			OnLeaveServer();
		}
		break;
	default:
		break;
	}
}
unsigned int NetClient::WorkerThread()
{
	for (;;)
	{
		DWORD cbTransferred = 0;
		SESSION* session = NULL;
		OVERLAPPED* overlapped = NULL;

		BOOL ret = GetQueuedCompletionStatus(_hCompletionPort, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE);
		if (session == NULL && cbTransferred == 0 && overlapped == NULL)
		{
			// ���� ��ȣ�� ���� �����带 ����
			PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);
			break;
		}

		if (cbTransferred != 0)
		{
			if (session->recvOverlapped == overlapped)
			{
				// Recv �Ϸ� ���� ó��
				RecvRoutine(session, cbTransferred);
			}
			else if (session->sendOverlapped == overlapped)
			{
				// Send �Ϸ� ���� ó��
				SendRoutine(session, cbTransferred);
			}
			else
			{
				// �ٸ� �����忡�� ���� �񵿱� �޽��� ó��
				UserMessageProc(cbTransferred, (LPVOID)session);
				continue;
			}
		}

		if (InterlockedDecrement(&session->ioCount) == 0)
			ReleaseSession(session);
	}
	return 0;
}
unsigned int __stdcall NetClient::WrapWorkerThread(LPVOID lpParam)
{
	NetClient* client = (NetClient*)lpParam;
	return client->WorkerThread();
}

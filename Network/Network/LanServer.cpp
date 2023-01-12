#include "LanServer.h"
#include "Error.h"
#include "Protocol.h"
#include "User.h"
#include "Logger.h"
#include "Enviroment.h"
#include "NetException.h"
#include <process.h>
#include <synchapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

USEJAYNAMESPACE
LanServer::LanServer() : _sessionCnt(0), _sessionKey(0)
{
	WSADATA ws;
	int status = WSAStartup(MAKEWORD(2, 2), &ws);
	if (status != 0)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, status);
		return;
	}
}
LanServer::~LanServer()
{
	WSACleanup();
}
bool LanServer::Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, bool nagle)
{
	_workerCreateCnt = workerCreateCnt;
	_workerRunningCnt = workerRunningCnt;
	_sessionMax = sessionMax;

	//--------------------------------------------------------------------
	// Listen
	//--------------------------------------------------------------------
	if (!Listen(ipaddress, port, nagle))
		return false;

	//--------------------------------------------------------------------
	// Initial
	//--------------------------------------------------------------------
	if (!Initial())
	{
		closesocket(_listenSocket);
		return false;
	}

	//--------------------------------------------------------------------
	// Thread Begin
	//--------------------------------------------------------------------
	for (int i = 0; i < _workerCreateCnt; i++)
		_hWorkerThread[i] = (HANDLE)_beginthreadex(NULL, 0, WrapWorkerThread, this, 0, NULL);
	_hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, WrapAcceptThread, this, 0, NULL);
	_hMonitoringThread = (HANDLE)_beginthreadex(NULL, 0, WrapMonitoringThread, this, 0, NULL);

	return true;
}
void LanServer::Stop()
{
	//--------------------------------------------------------------------
	// Exit Thread Event
	//--------------------------------------------------------------------
	closesocket(_listenSocket);
	SetEvent(_hExitThreadEvent);
	PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);

	//--------------------------------------------------------------------
	// Thread Exit Wait
	//--------------------------------------------------------------------
	HANDLE hHandle[2] = { _hAcceptThread, _hMonitoringThread };
	DWORD ret;
	ret = WaitForMultipleObjects(2, hHandle, TRUE, INFINITE);
	if (ret == WAIT_FAILED)
		OnError(NET_ERROR_RELEASE_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());

	ret = WaitForMultipleObjects(_workerCreateCnt, _hWorkerThread, TRUE, INFINITE);
	if (ret == WAIT_FAILED)
		OnError(NET_ERROR_RELEASE_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());

	//--------------------------------------------------------------------
	// Release
	//--------------------------------------------------------------------
	Release();
}
bool LanServer::Disconnect(DWORD64 sessionID)
{
	SESSION* session = DuplicateSession(sessionID);
	if (session == nullptr)
		return false;

	DisconnectSession(session);
	CloseSession(session);
	return true;
}
bool LanServer::SendPacket(DWORD64 sessionID, NetPacketPtr packet)
{
	SESSION* session = DuplicateSession(sessionID);
	if (session == nullptr)
		return false;

	TrySendPacket(session, *packet);
	CloseSession(session);
	return true;
}
int LanServer::GetSessionCount()
{
	return _sessionCnt;
}
int LanServer::GetUsePacketCount()
{
	return NetPacket::_packetPool.GetUseCount();
}
int LanServer::GetAcceptTPS()
{
	return _monitoring.oldTPS.accept;
}
int LanServer::GetRecvTPS()
{
	return _monitoring.oldTPS.recv;
}
int LanServer::GetSendTPS()
{
	return _monitoring.oldTPS.send;
}
bool LanServer::Listen(const wchar_t* ipaddress, int port, bool nagle)
{
	_listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_listenSocket == INVALID_SOCKET)
	{
		OnError(NET_ERROR_LISTEN_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		return false;
	}

	linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 0;
	int ret = setsockopt(_listenSocket, SOL_SOCKET, SO_LINGER, (char*)&so_linger, sizeof(so_linger));
	if (ret == SOCKET_ERROR)
	{
		OnError(NET_ERROR_LISTEN_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	if (!nagle)
	{
		int option = TRUE;
		ret = setsockopt(_listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&option, sizeof(option));
		if (ret == SOCKET_ERROR)
		{
			OnError(NET_ERROR_LISTEN_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
			closesocket(_listenSocket);
			return false;
		}
	}

	SOCKADDR_IN listenAddr;
	ZeroMemory(&listenAddr, sizeof(listenAddr));
	listenAddr.sin_family = AF_INET;
	listenAddr.sin_port = htons(port);
	InetPton(AF_INET, ipaddress, &listenAddr.sin_addr);
	ret = bind(_listenSocket, (SOCKADDR*)&listenAddr, sizeof(listenAddr));
	if (ret == SOCKET_ERROR)
	{
		OnError(NET_ERROR_LISTEN_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	ret = listen(_listenSocket, SOMAXCONN_HINT(65535));
	if (ret == SOCKET_ERROR)
	{
		OnError(NET_ERROR_LISTEN_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	return true;
}
bool LanServer::Initial()
{
	memset(&_monitoring, 0, sizeof(MONITORING));

	size_t maxUserAddress = GetMaximumUserAddress();
	if (maxUserAddress != MAXIMUM_ADDRESS_RANGE)
	{
		OnError(NET_ERROR_INITIAL_FAILED, __FUNCTIONW__, __LINE__, NULL, maxUserAddress);
		return false;
	}

	_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, _workerRunningCnt);
	if (_hCompletionPort == NULL)
	{
		OnError(NET_ERROR_INITIAL_FAILED, __FUNCTIONW__, __LINE__, NULL, GetLastError());
		return false;
	}

	_hExitThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_hExitThreadEvent == NULL)
	{
		OnError(NET_ERROR_INITIAL_FAILED, __FUNCTIONW__, __LINE__, NULL, GetLastError());
		CloseHandle(_hCompletionPort);
		return false;
	}

	_hWorkerThread = new HANDLE[_workerCreateCnt];
	_sessionArray = new SESSION[_sessionMax];

	for (int idx = _sessionMax - 1; idx >= 0; idx--)
		_indexStack.Push(idx);

	return true;
}
void LanServer::Release()
{
	SESSION* session;
	for (int idx = 0; idx < _sessionMax; idx++)
	{
		session = &_sessionArray[idx];
		if (session->releaseFlag == FALSE)
			ReleaseSession(session);
	}

	WORD index;
	while (_indexStack.size() > 0)
		_indexStack.Pop(index);

	CloseHandle(_hCompletionPort);
	CloseHandle(_hExitThreadEvent);
	CloseHandle(_hMonitoringThread);
	CloseHandle(_hAcceptThread);
	for (int i = 0; i < _workerCreateCnt; i++)
		CloseHandle(_hWorkerThread[i]);

	delete[] _hWorkerThread;
	delete[] _sessionArray;
}
SESSION* LanServer::CreateSession(SOCKET socket, const wchar_t* ipaddress, int port)
{
	WORD index;
	if (!_indexStack.Pop(index))
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, NULL, NULL);
		return nullptr;
	}

	SESSION* session = &_sessionArray[index];
	InterlockedIncrement(&session->ioCount);

	session->sessionID = MAKE_SESSIONID(++_sessionKey, index);
	session->socket = socket;
	wcscpy_s(session->ip, ipaddress);
	session->port = port;
	session->recvQ.ClearBuffer();
	session->sendBufCount = 0;
	session->sendFlag = FALSE;
	session->disconnectFlag = FALSE;
	session->releaseFlag = FALSE;

	InterlockedIncrement16((SHORT*)&_sessionCnt);
	return session;
}
void LanServer::ReleaseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ������ IOCount, releaseFlag �� ��� 0 ���� Ȯ��
	//--------------------------------------------------------------------
	if (InterlockedCompareExchange64(&session->release, TRUE, FALSE) != FALSE)
		return;

	ClearSendPacket(session);
	OnClientLeave(session->sessionID);

	closesocket(session->socket);
	session->socket = INVALID_SOCKET;

	WORD index = GET_SESSION_INDEX(session->sessionID);
	_indexStack.Push(index);

	InterlockedDecrement16((SHORT*)&_sessionCnt);
}
void LanServer::DisconnectSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���Ͽ� ���� ��û�Ǿ��ִ� ��� IO �� �ߴ�
	//--------------------------------------------------------------------
	if (InterlockedExchange((LONG*)&session->disconnectFlag, TRUE) == TRUE)
		CancelIoEx((HANDLE)session->socket, NULL);
}
SESSION* LanServer::DuplicateSession(DWORD64 sessionID)
{
	WORD index = GET_SESSION_INDEX(sessionID);
	SESSION* session = &_sessionArray[index];
	InterlockedIncrement(&session->ioCount);

	do
	{
		//--------------------------------------------------------------------
		// ������� �������� Ȯ��
		//--------------------------------------------------------------------
		if (session->releaseFlag == TRUE)
			break;

		//--------------------------------------------------------------------
		// ����� �������� Ȯ��
		//--------------------------------------------------------------------
		if (session->sessionID != sessionID)
			break;

		//--------------------------------------------------------------------
		// ���� ã�� ����
		//--------------------------------------------------------------------
		return session;
	} while (0);

	//--------------------------------------------------------------------
	// ���� ã�� ����
	//--------------------------------------------------------------------
	CloseSession(session);
	return nullptr;
}
void LanServer::CloseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���� ���� ��ȯ
	//--------------------------------------------------------------------
	if (InterlockedDecrement(&session->ioCount) == 0)
		ReleaseSession(session);
}
void LanServer::RecvPost(SESSION* session)
{
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
	int freeSize = session->recvQ.GetFreeSize();

	//--------------------------------------------------------------------
	// WSARecv �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(&session->recvOverlapped, sizeof(session->recvOverlapped));
	WSABUF wsaRecvBuf[2];
	int recvBufCount = 1;

	wsaRecvBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaRecvBuf[0].len = directSize;
	if (freeSize > directSize)
	{
		wsaRecvBuf[1].len = freeSize - directSize;
		wsaRecvBuf[1].buf = session->recvQ.GetBufferPtr();
		recvBufCount++;
	}

	//--------------------------------------------------------------------
	// WSARecv ó��
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	DWORD flag = 0;
	int ret = WSARecv(session->socket, wsaRecvBuf, recvBufCount, NULL, &flag, &session->recvOverlapped, NULL);
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
			CancelIoEx((HANDLE)session->socket, &session->recvOverlapped);
	}
}
void LanServer::SendPost(SESSION* session)
{
	if (session->disconnectFlag == TRUE)
		return;

	for (;;)
	{
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
	ZeroMemory(&session->sendOverlapped, sizeof(session->sendOverlapped));
	WSABUF wsaSendBuf[MAX_SENDBUF];

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
		wsaSendBuf[count].len = packet->GetPacketSize();
		wsaSendBuf[count].buf = packet->GetHeaderPtr();
	}
	session->sendBufCount = count;

	//--------------------------------------------------------------------
	// WSASend ó��
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	int ret = WSASend(session->socket, wsaSendBuf, session->sendBufCount, NULL, 0, &session->sendOverlapped, NULL);
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
			CancelIoEx((HANDLE)session->socket, &session->sendOverlapped);
	}
}
void LanServer::CompleteRecvPacket(SESSION* session)
{
	LAN_PACKET_HEADER header;
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
		// ���ſ� �������� ����� Header + Payload ũ�� ��ŭ �ִ��� Ȯ��
		//--------------------------------------------------------------------
		if (session->recvQ.GetUseSize() < sizeof(header) + header.len)
			break;

		session->recvQ.MoveFront(sizeof(header));

		//--------------------------------------------------------------------
		// ����ȭ ���ۿ� Payload ���
		//--------------------------------------------------------------------
		NetPacketPtr packet;
		ret = session->recvQ.Dequeue(packet->GetRearBufferPtr(), header.len);
		if (ret != header.len)
		{
			Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, dequeue size: %d, ret size: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE, header.len, ret);
			OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			break;
		}
		packet->MoveRear(ret);

		try
		{
			//--------------------------------------------------------------------
			// ������ �ο� Payload �� ���� ����ȭ ���� ����
			//--------------------------------------------------------------------
			OnRecv(session->sessionID, packet);
		}
		catch (NetException& ex)
		{
			OnError(ex.GetLastError(), __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			return;
		}

		InterlockedIncrement(&_monitoring.curTPS.recv);
	}
}
void LanServer::CompleteSendPacket(SESSION* session)
{
	NetPacket* packet;
	int count;
	for (count = 0; count < session->sendBufCount; count++)
	{
		//--------------------------------------------------------------------
		// ���� �Ϸ��� ����ȭ ���� ����
		//--------------------------------------------------------------------
		packet = session->sendBuf[count];
		if (packet->DecrementRefCount() == 0)
			NetPacket::Free(packet);
	}

	InterlockedAdd(&_monitoring.curTPS.send, session->sendBufCount);
	session->sendBufCount = 0;
}
void LanServer::TrySendPacket(SESSION* session, NetPacket* packet)
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
	// ����ȭ ���ۿ� ��Ʈ��ũ �� ��� ���
	//--------------------------------------------------------------------
	LAN_PACKET_HEADER header;
	header.len = packet->GetUseSize();
	packet->PutHeader((char*)&header, sizeof(LAN_PACKET_HEADER));

	//--------------------------------------------------------------------
	// �۽ſ� ť�� ����ȭ ������ ������ ���
	//--------------------------------------------------------------------
	packet->IncrementRefCount();
	session->sendQ.Enqueue(packet);

	//--------------------------------------------------------------------
	// �۽� ��û
	//--------------------------------------------------------------------
	MessagePost(UM_SEND_PACKET, (LPVOID)session->sessionID);
	//SendPost(session);
}
void LanServer::ClearSendPacket(SESSION* session)
{
	NetPacket* packet;
	int count;
	for (count = 0; count < session->sendBufCount; count++)
	{
		//--------------------------------------------------------------------
		// ���� ������̴� ����ȭ ���� ����
		//--------------------------------------------------------------------
		packet = session->sendBuf[count];
		if (packet->DecrementRefCount() == 0)
			NetPacket::Free(packet);
	}
	session->sendBufCount = 0;

	while (session->sendQ.size() > 0)
	{
		//--------------------------------------------------------------------
		// �۽ſ� ť�� �����ִ� ����ȭ ���� ����
		//--------------------------------------------------------------------
		session->sendQ.Dequeue(packet);
		if (packet->DecrementRefCount() == 0)
			NetPacket::Free(packet);
	}
}
void LanServer::MessagePost(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// WorkerThread ���� Job ��û
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, message, (ULONG_PTR)lpParam, NULL);
}
void LanServer::MessageProc(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// �ٸ� ������κ��� ��û���� Job ó��
	//--------------------------------------------------------------------
	switch (message)
	{
		case UM_SEND_PACKET:
		{
			DWORD64 sessionID = (DWORD64)lpParam;
			SESSION* session = DuplicateSession(sessionID);
			if (session != nullptr)
			{
				SendPost(session);
				CloseSession(session);
			}
		}
		break;
	default:
		break;
	}
}
unsigned int LanServer::AcceptThread()
{
	for (;;)
	{
		//--------------------------------------------------------------------
		// Accept ó��. ó���� ���� ���ٸ� Block
		//--------------------------------------------------------------------
		SOCKADDR_IN clientAddr;
		int clientSize = sizeof(clientAddr);
		ZeroMemory(&clientAddr, clientSize);
		SOCKET client = accept(_listenSocket, (SOCKADDR*)&clientAddr, &clientSize);
		if (client == INVALID_SOCKET)
			break;

		//--------------------------------------------------------------------
		// ���������� �� Ȯ��. �ִ�ġ�� �ʰ��� ��� ���� ����
		//--------------------------------------------------------------------
		if (_sessionCnt >= _sessionMax)
		{
			closesocket(client);
			continue;
		}

		//--------------------------------------------------------------------
		// ������ �ο� �ű� �������� IP, Port �� �����Ͽ� ������ ����� ������ Ȯ��
		//--------------------------------------------------------------------
		wchar_t ip[16];
		int port;
		InetNtop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip) / 2);
		port = ntohs(clientAddr.sin_port);
		if (!OnConnectionRequest(ip, port))
		{
			// ������ �ο��� ������ ������� �ʴ´ٸ� ������ ���´�.
			closesocket(client);
			continue;
		}

		//--------------------------------------------------------------------
		// �ű� �������� ������ ����� IOCP �� ���
		//--------------------------------------------------------------------
		SESSION* session = CreateSession(client, ip, port);
		if (session == nullptr)
		{
			closesocket(client);
			continue;
		}

		//int sendBufSize = 0;
		//setsockopt(session->socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
		CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)session, NULL);

		//--------------------------------------------------------------------
		// �ű� �������� ������ ������ �ο� �˸��� ���� ���
		//--------------------------------------------------------------------
		OnClientJoin(session->sessionID);
		RecvPost(session);

		//--------------------------------------------------------------------
		// AcceptThread ���� �����ϴ� ���� ��ȯ
		//--------------------------------------------------------------------
		CloseSession(session);

		InterlockedIncrement(&_monitoring.curTPS.accept);
	}
	return 0;
}
unsigned int LanServer::WorkerThread()
{
	for (;;)
	{
		//--------------------------------------------------------------------
		// Recv, Send �Ϸ� ���� ó��. ó���� ���� ���ٸ� Block
		//--------------------------------------------------------------------
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

		if (cbTransferred == 0)
		{
			//--------------------------------------------------------------------
			// Recv, Send ���� ���� ó��
			//--------------------------------------------------------------------
			if (ret == FALSE)
			{
				DWORD err = GetLastError();
				switch (err)
				{
				case ERROR_NETNAME_DELETED:
				case WSAECONNABORTED:
				case WSAECONNRESET:
					break;
				default:
					OnError(NET_ERROR_SOCKET_FAILED, __FUNCTIONW__, __LINE__, session->sessionID, err);
					break;
				}
			}
		}
		else
		{
			//--------------------------------------------------------------------
			// Recv, Send �Ϸ� ���� ó��
			//--------------------------------------------------------------------
			if (&session->recvOverlapped == overlapped)
			{
				// Recv �Ϸ� ���� ó��
				session->recvQ.MoveRear(cbTransferred);
				CompleteRecvPacket(session);
				RecvPost(session);
			}
			else if (&session->sendOverlapped == overlapped)
			{
				// Send �Ϸ� ���� ó��
				CompleteSendPacket(session);
				InterlockedExchange((LONG*)&session->sendFlag, FALSE);
				if (session->sendQ.size() > 0)
					SendPost(session);
			}
			else
			{
				// �ٸ� �����忡�� WorkerThread �� ���� �޽��� ó��
				MessageProc(cbTransferred, (LPVOID)session);
				continue;
			}
		}

		//--------------------------------------------------------------------
		// ioCount�� 0�̶�� ���� �������� ���� ������ ������ ��. ������ �����Ѵ�.
		//--------------------------------------------------------------------
		if (InterlockedDecrement(&session->ioCount) == 0)
			ReleaseSession(session);
	}
	return 0;
}
unsigned int LanServer::MonitoringThread()
{
	for (;;)
	{
		DWORD ret = WaitForSingleObject(_hExitThreadEvent, 1000);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_TIMEOUT:
			_monitoring.oldTPS.accept = InterlockedExchange(&_monitoring.curTPS.accept, 0);
			_monitoring.oldTPS.recv = InterlockedExchange(&_monitoring.curTPS.recv, 0);
			_monitoring.oldTPS.send = InterlockedExchange(&_monitoring.curTPS.send, 0);
			break;
		default:
			break;
		}
	}
	return 0;
}
unsigned int __stdcall LanServer::WrapAcceptThread(LPVOID lpParam)
{
	LanServer* server = (LanServer*)lpParam;
	return server->AcceptThread();
}
unsigned int __stdcall LanServer::WrapWorkerThread(LPVOID lpParam)
{
	LanServer* server = (LanServer*)lpParam;
	return server->WorkerThread();
}
unsigned int __stdcall LanServer::WrapMonitoringThread(LPVOID lpParam)
{
	LanServer* server = (LanServer*)lpParam;
	return server->MonitoringThread();
}
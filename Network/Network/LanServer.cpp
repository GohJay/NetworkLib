#include "LanServer.h"
#include "Protocol.h"
#include "define.h"
#include "Logger.h"
#include "CrashDump.h"
#include <process.h>
#include <synchapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

#define SESSIONID_INDEX_MASK		0x000000000000FFFF
#define SESSIONID_KEY_MASK			0x7FFFFFFFFFFF0000
#define SESSIONID_INVALIDBIT_MASK	0x8000000000000000
#define MAKE_SESSIONID(key, index)	(key << 16) | index
#define USER_MESSAGE_SEND			0x01

USEJAYNAMESPACE
LanServer::LanServer() : _sessionKey(0)
{
	InitializeSRWLock(&_indexLock);

	WSADATA ws;
	int status = WSAStartup(MAKEWORD(2, 2), &ws);
	if (status != 0)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() Failed WSAStartup - error: %d", __FUNCTIONW__, status);
		return;
	}
}
LanServer::~LanServer()
{
	WSACleanup();
}
bool LanServer::Start(wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, bool nagle)
{
	_workerCreateCnt = workerCreateCnt;
	_workerRunningCnt = workerRunningCnt;
	_sessionMax = sessionMax;
	_sessionCnt = 0;

	//--------------------------------------------------------------------
	// Listen
	//--------------------------------------------------------------------
	if (!Listen(ipaddress, port, nagle))
		return false;

	//--------------------------------------------------------------------
	// Initialize
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
	WaitForMultipleObjects(2, hHandle, TRUE, INFINITE);
	WaitForMultipleObjects(_workerCreateCnt, _hWorkerThread, TRUE, INFINITE);

	Release();
}
bool LanServer::Disconnect(DWORD64 sessionID)
{
	return false;
}
bool LanServer::SendPacket(DWORD64 sessionID, SerializationBuffer* packet)
{
	SESSION* session = AcquireSessionLock(sessionID);
	if (session == nullptr)
		return false;

	LAN_PACKET_HEADER header;
	MakeHeader(&header, packet);
	SendUnicast(session, &header, packet);
	SendPost(session);
	ReleaseSessionLock(session);

	InterlockedIncrement(&_monitoring.curTPS.send);
	return true;
}
int LanServer::GetSessionCount()
{
	return _sessionCnt;
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
LanServer::SESSION* LanServer::CreateSession(SOCKET socket, wchar_t* ipaddress, int port)
{
	WORD index;
	AcquireSRWLockExclusive(&_indexLock);
	index = _indexStack.top();
	_indexStack.pop();
	ReleaseSRWLockExclusive(&_indexLock);

	SESSION* session = &_sessionArray[index];
	session->ioCount = 0;
	session->socket = socket;
	wcscpy_s(session->ip, ipaddress);
	session->port = port;
	session->sendFlag = FALSE;
	session->recvQ.ClearBuffer();
	session->sendQ.ClearBuffer();
	session->sessionID = MAKE_SESSIONID(++_sessionKey, index);

	InterlockedIncrement16((SHORT*)&_sessionCnt);
	return session;
}
void LanServer::DisconnectSession(SESSION* session)
{
	DWORD64 sessionID = session->sessionID;
	WORD index = sessionID & SESSIONID_INDEX_MASK;

	AcquireSRWLockExclusive(&session->lock);
	session->sessionID |= SESSIONID_INVALIDBIT_MASK;
	ReleaseSRWLockExclusive(&session->lock);
	closesocket(session->socket);

	AcquireSRWLockExclusive(&_indexLock);
	_indexStack.push(index);
	ReleaseSRWLockExclusive(&_indexLock);

	OnClientLeave(sessionID);
	InterlockedDecrement16((SHORT*)&_sessionCnt);
}
void LanServer::RecvPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���ſ� �������� ������ ���ϱ�
	//--------------------------------------------------------------------
	int freeSize = session->recvQ.GetFreeSize();
	int directEnqueueSize = session->recvQ.DirectEnqueueSize();
	if (directEnqueueSize <= 0)
	{
		// ���������� ���� ���ٴ� ���� �����ۿ� ��� �޽����� ������ �ο��� �Ľ� �� �� ���� ������ �ִ� ��. ������ ���´�.
		OnError(NETWORK_ERROR_NETBUFFER_OVER, L"%s() Failed recvQ Enqueue - sessionID: %llu, size 0", __FUNCTIONW__, session->sessionID);		
		return;
	}

	//--------------------------------------------------------------------
	// WSARecv �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(&session->recvOverlapped, sizeof(session->recvOverlapped));
	WSABUF wsaRecvBuf[2];
	int recvBufCount = 1;
	wsaRecvBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaRecvBuf[0].len = directEnqueueSize;
	if (freeSize > directEnqueueSize)
	{
		wsaRecvBuf[1].buf = session->recvQ.GetBufferPtr();
		wsaRecvBuf[1].len = freeSize - directEnqueueSize;
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
				OnError(NETWORK_ERROR_SOCKET_FAILED, L"%s() Failed WSARecv - sessionID: %llu, error: %d", __FUNCTIONW__, session->sessionID, WSAGetLastError());
				break;
			}

			// ioCount�� 0�̶�� ���� ����
			if (InterlockedDecrement(&session->ioCount) == 0)
				DisconnectSession(session);
			return;
		}
	}
}
void LanServer::SendPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// �̹� ���� ���� Send ���� ������ return
	//--------------------------------------------------------------------
	if (InterlockedExchange((LONG*)&session->sendFlag, TRUE) == TRUE)
		return;

	//--------------------------------------------------------------------
	// �۽ſ� �������� ������ ���ϱ�
	//--------------------------------------------------------------------
	int useSize = session->sendQ.GetUseSize();
	int directDequeueSize = session->sendQ.DirectDequeueSize();
	if (directDequeueSize <= 0)
	{
		// Send ���տ� �����Ͽ� �̰����� �����Ͽ��� �����ۿ� ����ִ� �����Ͱ� ���տ� �����ϱ� ������ �ٸ� �����忡���� �̹� �۽ŵǾ��� �� �ִ�.
		// �̶��� 0 byte �� ������ ���� �ƴ϶� �۽��� ���� ó���� �ٽ����ش�.
		InterlockedExchange((LONG*)&session->sendFlag, FALSE);
		if (session->sendQ.GetUseSize() > 0)
			SendPost(session);
		return;
	}

	//--------------------------------------------------------------------
	// WSASend �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(&session->sendOverlapped, sizeof(session->sendOverlapped));
	WSABUF wsaSendBuf[2];
	int sendBufCount = 1;
	wsaSendBuf[0].buf = session->sendQ.GetFrontBufferPtr();
	wsaSendBuf[0].len = directDequeueSize;
	if (useSize > directDequeueSize)
	{
		wsaSendBuf[1].buf = session->sendQ.GetBufferPtr();
		wsaSendBuf[1].len = useSize - directDequeueSize;
		sendBufCount++;
	}

	//--------------------------------------------------------------------
	// WSASend ó��
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	int ret = WSASend(session->socket, wsaSendBuf, sendBufCount, NULL, 0, &session->sendOverlapped, NULL);
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
				OnError(NETWORK_ERROR_SOCKET_FAILED, L"%s() Failed WSASend - sessionID: %llu, error: %d", __FUNCTIONW__, session->sessionID, WSAGetLastError());
				break;
			}

			// ioCount�� 0�̶�� ���� ����
			if (InterlockedDecrement(&session->ioCount) == 0)
				DisconnectSession(session);
			return;
		}
	}
}
void LanServer::CompleteRecvPacket(SESSION* session)
{
	LAN_PACKET_HEADER header;
	int headerSize = sizeof(header);
	for (;;)
	{
		//--------------------------------------------------------------------
		// ���ſ� �������� ����� Header ũ�⺸�� ū�� Ȯ��
		//--------------------------------------------------------------------
		int size = session->recvQ.GetUseSize();
		if (size <= headerSize)
			break;

		//--------------------------------------------------------------------
		// ���ſ� �����ۿ��� Header �� Peek �Ͽ� Ȯ��
		//--------------------------------------------------------------------
		int ret = session->recvQ.Peek((char*)&header, headerSize);
		if (ret != headerSize)
			CrashDump::Crash();

		//--------------------------------------------------------------------
		// ���ſ� �������� ����� Header + Payload ũ�� ��ŭ �ִ��� Ȯ��
		//--------------------------------------------------------------------
		if (session->recvQ.GetUseSize() < headerSize + header.len)
			break;

		session->recvQ.MoveFront(headerSize);

		//--------------------------------------------------------------------
		// ����ȭ ���ۿ� Payload ���
		//--------------------------------------------------------------------
		SerializationBuffer packet;
		ret = session->recvQ.Dequeue(packet.GetBufferPtr(), header.len);
		if (ret != header.len)
			CrashDump::Crash();

		packet.MoveRear(ret);

		//--------------------------------------------------------------------
		// ������ �ο� Payload �� ���� ����ȭ ���� ����
		//--------------------------------------------------------------------
		OnRecv(session->sessionID, &packet);
		InterlockedIncrement(&_monitoring.curTPS.recv);
	}
}
void LanServer::SendUnicast(SESSION* session, LAN_PACKET_HEADER* header, Jay::SerializationBuffer* packet)
{
	//--------------------------------------------------------------------
	// �۽ſ� �����ۿ� ���� �޼��� ���
	//--------------------------------------------------------------------
	int size = sizeof(LAN_PACKET_HEADER);
	int ret = session->sendQ.Enqueue((char*)header, size);
	if (ret != size)
	{
		// ���������� �����Ͽ� �޽����� ���� �� ���ٴ� ���� ������ ó������ �� �ִ� �Ѱ踦 �����ٴ� ��. ������ ���´�.
		OnError(NETWORK_ERROR_NETBUFFER_OVER, L"%s() Failed sendQ Enqueue - sessionID: %llu, req size: %d, res size: %d", __FUNCTIONW__, session->sessionID, size, ret);
		return;
	}

	size = packet->GetUseSize();
	ret = session->sendQ.Enqueue(packet->GetBufferPtr(), size);
	if (ret != size)
	{
		// ���������� �����Ͽ� �޽����� ���� �� ���ٴ� ���� ������ ó������ �� �ִ� �Ѱ踦 �����ٴ� ��. ������ ���´�.
		OnError(NETWORK_ERROR_NETBUFFER_OVER, L"%s() Failed sendQ Enqueue - sessionID: %llu, req size: %d, res size: %d", __FUNCTIONW__, session->sessionID, size, ret);
		return;
	}
}
void LanServer::MakeHeader(LAN_PACKET_HEADER* header, SerializationBuffer* packet)
{
	header->len = packet->GetUseSize();
}
LanServer::SESSION* LanServer::AcquireSessionLock(DWORD64 sessionID)
{
	WORD index = sessionID & SESSIONID_INDEX_MASK;
	SESSION* session = &_sessionArray[index];
	AcquireSRWLockExclusive(&session->lock);
	if (session->sessionID != sessionID)
	{
		ReleaseSRWLockExclusive(&session->lock);
		return nullptr;
	}
	return session;
}
void LanServer::ReleaseSessionLock(SESSION* session)
{
	ReleaseSRWLockExclusive(&session->lock);
}
bool LanServer::Listen(wchar_t* ipaddress, int port, bool nagle)
{
	//--------------------------------------------------------------------
	// Server Listen
	//--------------------------------------------------------------------
	_listenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_listenSocket == INVALID_SOCKET)
	{
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed socket: %d", __FUNCTIONW__, WSAGetLastError());
		return false;
	}

	linger so_linger;
	so_linger.l_onoff = 1;
	so_linger.l_linger = 0;
	int ret = setsockopt(_listenSocket, SOL_SOCKET, SO_LINGER, (char*)&so_linger, sizeof(so_linger));
	if (ret == SOCKET_ERROR)
	{
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed socket: %d", __FUNCTIONW__, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	if (!nagle)
	{
		int option = TRUE;
		ret = setsockopt(_listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&option, sizeof(option));
		if (ret == SOCKET_ERROR)
		{
			OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed setsockopt nagle: %d", __FUNCTIONW__, WSAGetLastError());
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
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed bind: %d", __FUNCTIONW__, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	ret = listen(_listenSocket, SOMAXCONN_HINT(65535));
	if (ret == SOCKET_ERROR)
	{
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed listen: %d", __FUNCTIONW__, WSAGetLastError());
		closesocket(_listenSocket);
		return false;
	}

	return true;
}
bool LanServer::Initial()
{
	_monitoring.curTPS.accept = 0;
	_monitoring.curTPS.recv = 0;
	_monitoring.curTPS.send = 0;
	_monitoring.oldTPS.accept = 0;
	_monitoring.oldTPS.recv = 0;
	_monitoring.oldTPS.send = 0;

	_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, _workerRunningCnt);
	if (_hCompletionPort == NULL)
	{
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed CreateIoCompletionPort: %d", __FUNCTIONW__, GetLastError());
		return false;
	}

	_hExitThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_hExitThreadEvent == NULL)
	{
		OnError(NETWORK_ERROR_START_FAILED, L"%s() Failed CreateEvent: %d", __FUNCTIONW__, GetLastError());
		CloseHandle(_hCompletionPort);
		return false;
	}

	_hWorkerThread = new HANDLE[_workerCreateCnt];
	_sessionArray = new SESSION[_sessionMax];

	for (int idx = _sessionMax - 1; idx >= 0; idx--)
		_indexStack.push(idx);

	return true;
}
void LanServer::Release()
{
	while (_indexStack.size() > 0)
		_indexStack.pop();

	for (int idx = 0; idx < _sessionMax; idx++)
	{
		SESSION* session = &_sessionArray[idx];
		if ((session->sessionID & SESSIONID_INVALIDBIT_MASK) == 0)
			closesocket(session->socket);
	}

	CloseHandle(_hCompletionPort);
	CloseHandle(_hExitThreadEvent);
	CloseHandle(_hMonitoringThread);
	CloseHandle(_hAcceptThread);
	for (int i = 0; i < _workerCreateCnt; i++)
		CloseHandle(_hWorkerThread[i]);

	delete[] _hWorkerThread;
	delete[] _sessionArray;
}
void LanServer::MessageProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	//--------------------------------------------------------------------
	// ������ �ο��� WorkerThread �� ���� �޽��� ó��
	//--------------------------------------------------------------------
	switch (message)
	{
	case USER_MESSAGE_SEND:
		{
			DWORD64 sessionID = wParam;
			SESSION* session = AcquireSessionLock(sessionID);
			if (session != nullptr)
			{
				SendPost(session);
				ReleaseSessionLock(session);
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
		CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)session, NULL);

		//--------------------------------------------------------------------
		// �ű� �������� ������ ������ �ο� �˸�
		//--------------------------------------------------------------------
		OnClientJoin(session->sessionID);

		RecvPost(session);
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
					OnError(NETWORK_ERROR_SOCKET_FAILED, L"%s() Failed GetQueuedCompletionStatus - sessionID: %llu, error: %d", __FUNCTIONW__, session->sessionID, WSAGetLastError());
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
				session->sendQ.MoveFront(cbTransferred);
				InterlockedExchange((LONG*)&session->sendFlag, FALSE);
				if (session->sendQ.GetUseSize() > 0)
					SendPost(session);
			}
			else
			{
				// ������ �ο��� WorkerThread �� ���� �޽��� ó��
				MessageProc(cbTransferred, (WPARAM)session, (LPARAM)overlapped);
				continue;
			}
		}

		//--------------------------------------------------------------------
		// ioCount�� 0�̶�� ���� �������� ���� ������ ������ ��. ������ �����Ѵ�.
		//--------------------------------------------------------------------
		if (InterlockedDecrement(&session->ioCount) == 0)
			DisconnectSession(session);
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
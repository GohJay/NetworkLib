#include "NetServer.h"
#include "Error.h"
#include "Protocol.h"
#include "User.h"
#include "Logger.h"
#include "Enviroment.h"
#include "NetException.h"
#include <process.h>
#include <ws2tcpip.h>
#include <timeapi.h>
#pragma comment(lib, "ws2_32")

using namespace Jay;

NetServer::NetServer() : _sessionCnt(0), _sessionKey(0), _lastTimeoutProc(0)
{
	WSADATA ws;
	int status = WSAStartup(MAKEWORD(2, 2), &ws);
	if (status != 0)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, status);
		return;
	}
}
NetServer::~NetServer()
{
	WSACleanup();
}
bool NetServer::Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, BYTE packetCode, BYTE packetKey, int timeoutSec, bool nagle)
{
	_workerCreateCnt = workerCreateCnt;
	_workerRunningCnt = workerRunningCnt;
	_sessionMax = sessionMax;
	_timeoutSec = timeoutSec;
	_packetCode = packetCode;
	_packetKey = packetKey;
	ZeroMemory(&_monitoring, sizeof(MONITORING));

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
	_hManagementThread = (HANDLE)_beginthreadex(NULL, 0, WrapManagementThread, this, 0, NULL);

	return true;
}
void NetServer::Stop()
{
	//--------------------------------------------------------------------
	// AcceptThread, ManagementThread ���� ��ȣ ������
	//--------------------------------------------------------------------
	closesocket(_listenSocket);
	SetEvent(_hExitThreadEvent);

	//--------------------------------------------------------------------
	// AcceptThread, ManagementThread ���� ���
	//--------------------------------------------------------------------
	HANDLE hHandle[2] = { _hAcceptThread, _hManagementThread };
	DWORD ret;
	ret = WaitForMultipleObjects(2, hHandle, TRUE, INFINITE);
	if (ret == WAIT_FAILED)
		OnError(NET_ERROR_RELEASE_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());

	//--------------------------------------------------------------------
	// �����ִ� ��� ���� ����
	//--------------------------------------------------------------------
	SESSION* session;
	for (int index = 0; index < _sessionMax; index++)
	{
		session = &_sessionArray[index];
		if (session->releaseFlag != TRUE)
			DisconnectSession(session);
	}

	//--------------------------------------------------------------------
	// ��� ���� ���� ���
	//--------------------------------------------------------------------
	while (_sessionCnt > 0)
		Sleep(500);

	//--------------------------------------------------------------------
	// WorkerThread ���� ��ȣ ������
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);

	//--------------------------------------------------------------------
	// WorkerThread ���� ���
	//--------------------------------------------------------------------
	ret = WaitForMultipleObjects(_workerCreateCnt, _hWorkerThread, TRUE, INFINITE);
	if (ret == WAIT_FAILED)
		OnError(NET_ERROR_RELEASE_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());

	//--------------------------------------------------------------------
	// Release
	//--------------------------------------------------------------------
	Release();
}
bool NetServer::Disconnect(DWORD64 sessionID)
{
	SESSION* session = DuplicateSession(sessionID);
	if (session != nullptr)
	{
		DisconnectSession(session);
		CloseSession(session);
		return true;
	}
	return false;
}
bool NetServer::SendPacket(DWORD64 sessionID, NetPacket* packet)
{
	SESSION* session = DuplicateSession(sessionID);
	if (session != nullptr)
	{
		TrySendPacket(session, packet);
		CloseSession(session);
		return true;
	}
	return false;
}
int NetServer::GetSessionCount()
{
	return _sessionCnt;
}
int NetServer::GetUsePacketCount()
{
	return NetPacket::_packetPool.GetUseCount();
}
int NetServer::GetAcceptTPS()
{
	return _monitoring.oldTPS.accept;
}
int NetServer::GetRecvTPS()
{
	return _monitoring.oldTPS.recv;
}
int NetServer::GetSendTPS()
{
	return _monitoring.oldTPS.send;
}
__int64 NetServer::GetTotalAcceptCount()
{
	return _monitoring.acceptTotal;
}
SESSION* NetServer::CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr)
{
	WORD index;
	if (!_indexStack.Pop(index))
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, NULL, NULL);
		return nullptr;
	}

	SESSION* session = &_sessionArray[index];
	InterlockedIncrement16(&session->ioCount);

	memmove(&session->socketAddr, socketAddr, sizeof(SOCKADDR_IN));
	session->socket = socket;
	session->lastRecvTime = timeGetTime();
	session->recvQ.ClearBuffer();
	session->sendBufCount = 0;
	session->sendFlag = FALSE;
	session->disconnectFlag = FALSE;
	session->sessionID = MAKE_SESSIONID(++_sessionKey, index);
	session->releaseFlag = FALSE;

	InterlockedIncrement16((SHORT*)&_sessionCnt);
	return session;
}
void NetServer::ReleaseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ������ IOCount, releaseFlag �� ��� 0 ���� Ȯ��
	//--------------------------------------------------------------------
	if (InterlockedCompareExchange(&session->release, TRUE, FALSE) != FALSE)
		return;

	//--------------------------------------------------------------------
	// ���� �ڿ� ����
	//--------------------------------------------------------------------
	ClearSendPacket(session);
	closesocket(session->socket);

	//--------------------------------------------------------------------
	// ������ �ο� �˸� ��û
	//--------------------------------------------------------------------
	QueueUserMessage(UM_ALERT_CLIENT_LEAVE, (LPVOID)session->sessionID);

	WORD index = GET_SESSION_INDEX(session->sessionID);
	_indexStack.Push(index);

	InterlockedDecrement16((SHORT*)&_sessionCnt);
}
void NetServer::DisconnectSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���Ͽ� ���� ��û�Ǿ��ִ� ��� IO �� �ߴ�
	//--------------------------------------------------------------------
	if (InterlockedExchange8(&session->disconnectFlag, TRUE) == FALSE)
		CancelIoEx((HANDLE)session->socket, NULL);
}
SESSION* NetServer::DuplicateSession(DWORD64 sessionID)
{
	WORD index = GET_SESSION_INDEX(sessionID);
	SESSION* session = &_sessionArray[index];

	InterlockedIncrement16(&session->ioCount);

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
		// ã�� ���� ȹ��
		//--------------------------------------------------------------------
		return session;
	} while (0);

	if (InterlockedDecrement16(&session->ioCount) == 0)
		ReleaseSession(session);

	return nullptr;
}
void NetServer::CloseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// ���� ���� ��ȯ
	//--------------------------------------------------------------------
	if (InterlockedDecrement16(&session->ioCount) == 0)
		ReleaseSession(session);
}
void NetServer::RecvPost(SESSION* session)
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
	ZeroMemory(&session->recvOverlapped, sizeof(OVERLAPPED));
	WSABUF wsaBuf[2];

	wsaBuf[0].len = directSize;
	wsaBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaBuf[1].len = session->recvQ.GetFreeSize() - directSize;
	wsaBuf[1].buf = session->recvQ.GetBufferPtr();

	//--------------------------------------------------------------------
	// WSARecv ó��
	//--------------------------------------------------------------------
	InterlockedIncrement16(&session->ioCount);
	DWORD flag = 0;
	int ret = WSARecv(session->socket, wsaBuf, 2, NULL, &flag, &session->recvOverlapped, NULL);
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
			if (InterlockedDecrement16(&session->ioCount) == 0)
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
void NetServer::SendPost(SESSION* session)
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
		if (InterlockedExchange8(&session->sendFlag, TRUE) == TRUE)
			return;

		if (session->sendQ.size() > 0)
			break;

		//--------------------------------------------------------------------
		// ������ �����Ͱ� �ִ��� ���� Ȯ�� �� ������ return
		//--------------------------------------------------------------------
		InterlockedExchange8(&session->sendFlag, FALSE);
		if (session->sendQ.size() <= 0)
			return;
	}

	//--------------------------------------------------------------------
	// WSASend �� ���� �Ű����� �ʱ�ȭ
	//--------------------------------------------------------------------
	ZeroMemory(&session->sendOverlapped, sizeof(OVERLAPPED));
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
	InterlockedIncrement16(&session->ioCount);
	int ret = WSASend(session->socket, wsaBuf, count, NULL, 0, &session->sendOverlapped, NULL);
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
			if (InterlockedDecrement16(&session->ioCount) == 0)
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
void NetServer::RecvRoutine(SESSION* session, DWORD cbTransferred)
{
	session->lastRecvTime = timeGetTime();
	session->recvQ.MoveRear(cbTransferred);
	CompleteRecvPacket(session);
	RecvPost(session);
}
void NetServer::SendRoutine(SESSION* session, DWORD cbTransferred)
{
	CompleteSendPacket(session);
	InterlockedExchange8(&session->sendFlag, FALSE);
	if (session->sendQ.size() > 0)
		SendPost(session);
}
void NetServer::CompleteRecvPacket(SESSION* session)
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
			OnError(NET_ERROR_DECODE_FAILED, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			NetPacket::Free(packet);
			break;
		}

		try
		{
			//--------------------------------------------------------------------
			// ������ �ο� ����ȭ ���� ����
			//--------------------------------------------------------------------
			OnRecv(session->sessionID, packet);
		}
		catch (NetException& ex)
		{
			OnError(ex.GetLastError(), __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			DisconnectSession(session);
			NetPacket::Free(packet);
			break;
		}

		NetPacket::Free(packet);
		InterlockedIncrement(&_monitoring.curTPS.recv);
	}
}
void NetServer::CompleteSendPacket(SESSION* session)
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

	InterlockedAdd(&_monitoring.curTPS.send, session->sendBufCount);
	session->sendBufCount = 0;
}
void NetServer::TrySendPacket(SESSION* session, NetPacket* packet)
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
	InterlockedIncrement16(&session->ioCount);
	QueueUserMessage(UM_POST_SEND_PACKET, (LPVOID)session);
}
void NetServer::ClearSendPacket(SESSION* session)
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
void NetServer::QueueUserMessage(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// WorkerThread ���� Job ��û
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, message, (ULONG_PTR)lpParam, NULL);
}
void NetServer::UserMessageProc(DWORD message, LPVOID lpParam)
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
	case UM_ALERT_CLIENT_LEAVE:
		{
			DWORD64 sessionID = (DWORD64)lpParam;
			OnClientLeave(sessionID);
		}
		break;
	default:
		break;
	}
}
void NetServer::TimeoutProc()
{
	//--------------------------------------------------------------------
	// Ÿ�Ӿƿ� ��� �ɼ��� OFF�� ��� return
	//--------------------------------------------------------------------
	if (_timeoutSec <= 0)
		return;

	//--------------------------------------------------------------------
	// Ÿ�Ӿƿ� ó�� �ð��� ���� �ƴ� ��� return
	//--------------------------------------------------------------------
	DWORD currentTime = timeGetTime();
	DWORD timeout = currentTime - (_timeoutSec * 1000);
	if (timeout < _lastTimeoutProc)
		return;

	SESSION* session;
	DWORD64 sessionID;
	for (int index = 0; index < _sessionMax; index++)
	{
		//--------------------------------------------------------------------
		// ������� �������� Ȯ��
		//--------------------------------------------------------------------
		session = &_sessionArray[index];
		if (session->releaseFlag == TRUE)
			continue;

		//--------------------------------------------------------------------
		// Ÿ�Ӿƿ� ���� �Ǵ�
		//--------------------------------------------------------------------
		sessionID = session->sessionID;
		if (session->lastRecvTime > timeout)
			continue;

		InterlockedIncrement16(&session->ioCount);

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
			// Ÿ�Ӿƿ� ó��
			//--------------------------------------------------------------------
			DisconnectSession(session);
		} while (0);

		if (InterlockedDecrement16(&session->ioCount) == 0)
			ReleaseSession(session);
	}

	_lastTimeoutProc = currentTime;
}
void NetServer::UpdateTPS()
{
	//--------------------------------------------------------------------
	// ����͸��� TPS ����
	//--------------------------------------------------------------------
	_monitoring.oldTPS.accept = InterlockedExchange(&_monitoring.curTPS.accept, 0);
	_monitoring.oldTPS.recv = InterlockedExchange(&_monitoring.curTPS.recv, 0);
	_monitoring.oldTPS.send = InterlockedExchange(&_monitoring.curTPS.send, 0);
}
bool NetServer::Listen(const wchar_t* ipaddress, int port, bool nagle)
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
bool NetServer::Initial()
{
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
	_sessionArray = (SESSION*)_aligned_malloc(sizeof(SESSION) * _sessionMax, 64);

	int index;
	for (index = 0; index < _sessionMax; index++)
		new(&_sessionArray[index]) SESSION();

	for (index = _sessionMax - 1; index >= 0; index--)
		_indexStack.Push(index);

	return true;
}
void NetServer::Release()
{
	WORD index;
	while (_indexStack.size() > 0)
		_indexStack.Pop(index);

	for (index = 0; index < _sessionMax; index++)
		_sessionArray[index].~SESSION();

	CloseHandle(_hCompletionPort);
	CloseHandle(_hExitThreadEvent);
	CloseHandle(_hManagementThread);
	CloseHandle(_hAcceptThread);
	for (int i = 0; i < _workerCreateCnt; i++)
		CloseHandle(_hWorkerThread[i]);

	_aligned_free(_sessionArray);
	delete[] _hWorkerThread;
}
unsigned int NetServer::AcceptThread()
{
	for (;;)
	{
		SOCKADDR_IN socketAddr;
		int socketAddrSize = sizeof(socketAddr);
		ZeroMemory(&socketAddr, socketAddrSize);

		//--------------------------------------------------------------------
		// Accept ó��. ó���� ���� ���ٸ� Block
		//--------------------------------------------------------------------
		SOCKET socket = accept(_listenSocket, (SOCKADDR*)&socketAddr, &socketAddrSize);
		if (socket == INVALID_SOCKET)
			break;

		//--------------------------------------------------------------------
		// ���������� �� Ȯ��. �ִ�ġ�� �ʰ��� ��� ���� ����
		//--------------------------------------------------------------------
		if (_sessionCnt >= _sessionMax)
		{
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// ������ �ο� �ű� �������� IP, Port �� �����Ͽ� ������ ����� ������ Ȯ��
		//--------------------------------------------------------------------
		wchar_t ip[16];
		int port;
		InetNtop(AF_INET, &socketAddr.sin_addr, ip, sizeof(ip) / 2);
		port = ntohs(socketAddr.sin_port);
		if (!OnConnectionRequest(ip, port))
		{
			// ������ �ο��� ������ ������� �ʴ´ٸ� ������ ���´�.
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// �ű� �������� ���� �Ҵ�
		//--------------------------------------------------------------------
		SESSION* session = CreateSession(socket, &socketAddr);
		if (session == nullptr)
		{
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// �Ҵ�� ������ IOCP�� ���
		//--------------------------------------------------------------------
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

		_monitoring.acceptTotal++;
		InterlockedIncrement(&_monitoring.curTPS.accept);
	}
	return 0;
}
unsigned int NetServer::WorkerThread()
{
	for (;;)
	{
		DWORD cbTransferred = 0;
		SESSION* session = NULL;
		OVERLAPPED* overlapped = NULL;

		BOOL ret = GetQueuedCompletionStatus(_hCompletionPort, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE);
		if (overlapped == NULL)
		{
			if (session == NULL && cbTransferred == 0)
			{
				// ���� ��ȣ�� ���� �����带 ����
				PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);
				break;
			}
			else
			{
				// �ٸ� �����忡�� ���� �񵿱� �޽��� ó��
				UserMessageProc(cbTransferred, (LPVOID)session);
				continue;
			}
		}

		if (cbTransferred != 0)
		{
			if (&session->recvOverlapped == overlapped)
			{
				// Recv �Ϸ� ���� ó��
				RecvRoutine(session, cbTransferred);
			}
			else if (&session->sendOverlapped == overlapped)
			{
				// Send �Ϸ� ���� ó��
				SendRoutine(session, cbTransferred);
			}
		}

		if (InterlockedDecrement16(&session->ioCount) == 0)
			ReleaseSession(session);
	}
	return 0;
}
unsigned int NetServer::ManagementThread()
{
	for (;;)
	{
		DWORD ret = WaitForSingleObject(_hExitThreadEvent, 1000);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			return 0;
		case WAIT_TIMEOUT:
			UpdateTPS();
			TimeoutProc();
			break;
		default:
			break;
		}
	}
	return 0;
}
unsigned int __stdcall NetServer::WrapAcceptThread(LPVOID lpParam)
{
	NetServer* server = (NetServer*)lpParam;
	return server->AcceptThread();
}
unsigned int __stdcall NetServer::WrapWorkerThread(LPVOID lpParam)
{
	NetServer* server = (NetServer*)lpParam;
	return server->WorkerThread();
}
unsigned int __stdcall NetServer::WrapManagementThread(LPVOID lpParam)
{
	NetServer* server = (NetServer*)lpParam;
	return server->ManagementThread();
}
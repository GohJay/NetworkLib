#include "LanServer.h"
#include "Error.h"
#include "Protocol.h"
#include "Logger.h"
#include <process.h>
#include <synchapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")

#define SESSIONID_INDEX_MASK		0x000000000000FFFF
#define SESSIONID_KEY_MASK			0x7FFFFFFFFFFF0000
#define SESSIONID_INVALIDBIT_MASK	0x8000000000000000
#define MAKE_SESSIONID(key, index)	(key << 16) | index

#define USER_MESSAGE_SEND			0x01
#define SEND_BUFFER_MAX				100

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

	InitializeSRWLock(&_indexLock);
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

	Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - session count: %d", __FUNCTIONW__, __LINE__, _sessionCnt);
	Logger::WriteLog(L"Net", LOG_LEVEL_SYSTEM, L"%s() line: %d - packet capacity: %d, packet useCount: %d", __FUNCTIONW__, __LINE__, 
		SerializationBuffer::_packetPool.GetCapacityCount(),
		SerializationBuffer::_packetPool.GetUseCount());
}
bool LanServer::Disconnect(DWORD64 sessionID)
{
	return false;
}
bool LanServer::SendPacket(DWORD64 sessionID, SerializationBuffer* packet)
{
	SESSION* session = AcquireSessionLock(sessionID);
	if (session != nullptr)
	{
		SendUnicast(session, packet);
		SendPost(session);
		ReleaseSessionLock(session);
		return true;
	}
	return false;
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
	_monitoring.curTPS.accept = 0;
	_monitoring.curTPS.recv = 0;
	_monitoring.curTPS.send = 0;
	_monitoring.oldTPS.accept = 0;
	_monitoring.oldTPS.recv = 0;
	_monitoring.oldTPS.send = 0;

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
		_indexStack.push(idx);

	return true;
}
void LanServer::Release()
{
	for (int idx = 0; idx < _sessionMax; idx++)
	{
		SESSION* session = &_sessionArray[idx];
		if ((session->sessionID & SESSIONID_INVALIDBIT_MASK) == 0)
			DisconnectSession(session);
	}

	while (_indexStack.size() > 0)
		_indexStack.pop();

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
	AcquireSRWLockExclusive(&_indexLock);
	index = _indexStack.top();
	_indexStack.pop();
	ReleaseSRWLockExclusive(&_indexLock);

	SESSION* session = &_sessionArray[index];
	session->socket = socket;
	wcscpy_s(session->ip, ipaddress);
	session->port = port;
	session->ioCount = 0;
	session->recvQ.ClearBuffer();
	session->sendQ.ClearBuffer();
	session->sendFlag = FALSE;
	session->sendBufCount = 0;
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
	CleanupSendBuffer(session);

	AcquireSRWLockExclusive(&_indexLock);
	_indexStack.push(index);
	ReleaseSRWLockExclusive(&_indexLock);

	OnClientLeave(sessionID);
	InterlockedDecrement16((SHORT*)&_sessionCnt);
}
void LanServer::RecvPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// 수신용 링버퍼의 사이즈 구하기
	//--------------------------------------------------------------------
	int freeSize = session->recvQ.GetFreeSize();
	int directEnqueueSize = session->recvQ.DirectEnqueueSize();
	if (directEnqueueSize <= 0)
	{
		// 여유공간이 전혀 없다는 것은 링버퍼에 담긴 메시지에 컨텐츠 부에서 파싱 할 수 없는 오류가 있는 것. 연결을 끊는다.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		return;
	}

	//--------------------------------------------------------------------
	// WSARecv 를 위한 매개변수 초기화
	//--------------------------------------------------------------------
	ZeroMemory(&session->recvOverlapped, sizeof(session->recvOverlapped));
	WSABUF wsaRecvBuf[2];
	int recvBufCount = 1;
	wsaRecvBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaRecvBuf[0].len = directEnqueueSize;
	if (freeSize > directEnqueueSize)
	{
		wsaRecvBuf[1].len = freeSize - directEnqueueSize;
		wsaRecvBuf[1].buf = session->recvQ.GetBufferPtr();
		recvBufCount++;
	}

	//--------------------------------------------------------------------
	// WSARecv 처리
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

			// ioCount가 0이라면 연결 끊기
			if (InterlockedDecrement(&session->ioCount) == 0)
				DisconnectSession(session);
			return;
		}
	}
}
void LanServer::SendPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// 이미 전송 중인 Send 건이 있으면 return
	//--------------------------------------------------------------------
	if (InterlockedExchange((LONG*)&session->sendFlag, TRUE) == TRUE)
		return;

	//--------------------------------------------------------------------
	// 송신용 링버퍼의 사이즈 구하기
	//--------------------------------------------------------------------
	int useSize = session->sendQ.GetUseSize();
	if (useSize <= 0)
	{
		// Send 경합에 성공하여 이곳까지 진입하여도 링버퍼에 담겨있던 데이터가 경합에 성공하기 직전에 다른 스레드에의해 이미 송신되었을 수 있다.
		// 이때는 0 byte 를 보내는 것이 아니라 송신을 위한 처리를 다시해준다.
		InterlockedExchange((LONG*)&session->sendFlag, FALSE);
		if (session->sendQ.GetUseSize() > 0)
			SendPost(session);
		return;
	}

	int packetCount = useSize / sizeof(void*);
	if (packetCount > SEND_BUFFER_MAX)
		packetCount = SEND_BUFFER_MAX;

	//--------------------------------------------------------------------
	// 송신할 직렬화 버퍼 포인터 Peek
	//--------------------------------------------------------------------
	SerializationBuffer* packet[SEND_BUFFER_MAX];
	int size = packetCount * sizeof(void*);
	int ret = session->sendQ.Peek((char*)packet, size);
	if (ret != size)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, peek size: %d, ret size: %d", NET_FATAL_INVALID_SIZE, size, ret);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		// Disconnect 처리
		// return;
	}

	//--------------------------------------------------------------------
	// WSASend 를 위한 매개변수 초기화
	//--------------------------------------------------------------------
	ZeroMemory(&session->sendOverlapped, sizeof(session->sendOverlapped));
	WSABUF wsaSendBuf[SEND_BUFFER_MAX];
	session->sendBufCount = packetCount;
	for (int i = 0; i < session->sendBufCount; i++)
	{
		wsaSendBuf[i].len = packet[i]->GetPacketSize();
		wsaSendBuf[i].buf = packet[i]->GetHeaderPtr();
	}

	//--------------------------------------------------------------------
	// WSASend 처리
	//--------------------------------------------------------------------
	InterlockedIncrement(&session->ioCount);
	ret = WSASend(session->socket, wsaSendBuf, session->sendBufCount, NULL, 0, &session->sendOverlapped, NULL);
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

			// ioCount가 0이라면 연결 끊기
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
		// 수신용 링버퍼의 사이즈가 Header 크기보다 큰지 확인
		//--------------------------------------------------------------------
		int size = session->recvQ.GetUseSize();
		if (size <= headerSize)
			break;

		//--------------------------------------------------------------------
		// 수신용 링버퍼에서 Header 를 Peek 하여 확인
		//--------------------------------------------------------------------
		int ret = session->recvQ.Peek((char*)&header, headerSize);
		if (ret != headerSize)
		{
			Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, peek size: %d, ret size: %d", NET_FATAL_INVALID_SIZE, headerSize, ret);
			OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			break;
		}

		//--------------------------------------------------------------------
		// 수신용 링버퍼의 사이즈가 Header + Payload 크기 만큼 있는지 확인
		//--------------------------------------------------------------------
		if (session->recvQ.GetUseSize() < headerSize + header.len)
			break;
		session->recvQ.MoveFront(headerSize);

		//--------------------------------------------------------------------
		// 직렬화 버퍼에 Payload 담기
		//--------------------------------------------------------------------
		SerializationBuffer* packet = SerializationBuffer::Alloc();
		ret = session->recvQ.Dequeue(packet->GetRearBufferPtr(), header.len);
		if (ret != header.len)
		{
			Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, dequeue size: %d, ret size: %d", NET_FATAL_INVALID_SIZE, header.len, ret);
			OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
			break;
		}
		packet->MoveRear(ret);

		//--------------------------------------------------------------------
		// 컨텐츠 부에 Payload 를 담은 직렬화 버퍼 전달
		//--------------------------------------------------------------------
		OnRecv(session->sessionID, packet);
		SerializationBuffer::Free(packet);

		InterlockedIncrement(&_monitoring.curTPS.recv);
	}
}
void LanServer::CompleteSendPacket(SESSION* session)
{
	//--------------------------------------------------------------------
	// 송신용 링버퍼에 있는 직렬화 버퍼 Dequeue
	//--------------------------------------------------------------------
	SerializationBuffer* packet[SEND_BUFFER_MAX];
	int size = session->sendBufCount * sizeof(void*);
	int ret = session->sendQ.Dequeue((char*)packet, size);
	if (ret != size)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, dequeue size: %d, ret size: %d", NET_FATAL_INVALID_SIZE, size, ret);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
	}

	//--------------------------------------------------------------------
	// 직렬화 버퍼 정리
	//--------------------------------------------------------------------
	while (session->sendBufCount > 0)
	{
		SerializationBuffer::Free(packet[session->sendBufCount - 1]);
		session->sendBufCount--;
		InterlockedIncrement(&_monitoring.curTPS.send);
	}
}
void LanServer::SendUnicast(SESSION* session, SerializationBuffer* packet)
{
	//--------------------------------------------------------------------
	// 네트워크 부 헤더 만들기
	//--------------------------------------------------------------------
	LAN_PACKET_HEADER header;
	header.len = packet->GetUseSize();
	packet->PutHeader((char*)&header, sizeof(LAN_PACKET_HEADER));

	//--------------------------------------------------------------------
	// 송신용 링버퍼에 보낼 직렬화 버퍼의 포인터 담기
	//--------------------------------------------------------------------
	int size = sizeof(void*);
	int ret = session->sendQ.Enqueue((char*)&packet, size);
	if (ret != size)
	{
		// 여유공간이 부족하여 메시지를 담을 수 없다는 것은 서버가 처리해줄 수 있는 한계를 지났다는 것. 연결을 끊는다.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, size);
		return;
	}
}
void LanServer::CleanupSendBuffer(SESSION* session)
{
	int size = sizeof(void*);
	while (session->sendQ.GetUseSize() >= size)
	{
		//--------------------------------------------------------------------
		// 송신용 링버퍼에 있는 직렬화 버퍼 Dequeue
		//--------------------------------------------------------------------
		SerializationBuffer* packet[SEND_BUFFER_MAX];
		int len = size * SEND_BUFFER_MAX;
		int ret = session->sendQ.Dequeue((char*)packet, len);

		//--------------------------------------------------------------------
		// 직렬화 버퍼 정리
		//--------------------------------------------------------------------
		int packetCount = ret / size;
		for (int i = 0; i < packetCount; i++)
			SerializationBuffer::Free(packet[i]);
	}
}
SESSION* LanServer::AcquireSessionLock(DWORD64 sessionID)
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
void LanServer::MessageProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	//--------------------------------------------------------------------
	// 다른 스레드에서 WorkerThread 에 보낸 메시지 처리
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
		// Accept 처리. 처리할 것이 없다면 Block
		//--------------------------------------------------------------------
		SOCKADDR_IN clientAddr;
		int clientSize = sizeof(clientAddr);
		ZeroMemory(&clientAddr, clientSize);
		SOCKET client = accept(_listenSocket, (SOCKADDR*)&clientAddr, &clientSize);
		if (client == INVALID_SOCKET)
			break;

		//--------------------------------------------------------------------
		// 동시접속자 수 확인. 최대치를 초과할 경우 연결 종료
		//--------------------------------------------------------------------
		if (_sessionCnt >= _sessionMax)
		{
			closesocket(client);
			continue;
		}

		//--------------------------------------------------------------------
		// 컨텐츠 부에 신규 접속자의 IP, Port 를 전달하여 접속을 허용할 것인지 확인
		//--------------------------------------------------------------------
		wchar_t ip[16];
		int port;
		InetNtop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip) / 2);
		port = ntohs(clientAddr.sin_port);
		if (!OnConnectionRequest(ip, port))
		{
			// 컨텐츠 부에서 접속을 허용하지 않는다면 연결을 끊는다.
			closesocket(client);
			continue;
		}

		//--------------------------------------------------------------------
		// 신규 접속자의 세션을 만들고 IOCP 에 등록
		//--------------------------------------------------------------------
		SESSION* session = CreateSession(client, ip, port);
		int sendBufSize = 0;
		setsockopt(session->socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
		CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)session, NULL);

		//--------------------------------------------------------------------
		// 신규 접속자의 정보를 컨텐츠 부에 알림
		//--------------------------------------------------------------------
		InterlockedIncrement(&session->ioCount);
		OnClientJoin(session->sessionID);
		RecvPost(session);
		if (InterlockedDecrement(&session->ioCount) == 0)
			DisconnectSession(session);

		InterlockedIncrement(&_monitoring.curTPS.accept);
	}
	return 0;
}
unsigned int LanServer::WorkerThread()
{
	for (;;)
	{
		//--------------------------------------------------------------------
		// Recv, Send 완료 통지 처리. 처리할 것이 없다면 Block
		//--------------------------------------------------------------------
		DWORD cbTransferred = 0;
		SESSION* session = NULL;
		OVERLAPPED* overlapped = NULL;
		BOOL ret = GetQueuedCompletionStatus(_hCompletionPort, &cbTransferred, (PULONG_PTR)&session, (LPOVERLAPPED*)&overlapped, INFINITE);
		if (session == NULL && cbTransferred == 0 && overlapped == NULL)
		{
			// 종료 신호가 오면 스레드를 종료
			PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);
			break;
		}

		if (cbTransferred == 0)
		{
			//--------------------------------------------------------------------
			// Recv, Send 오류 통지 처리
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
			// Recv, Send 완료 통지 처리
			//--------------------------------------------------------------------
			if (&session->recvOverlapped == overlapped)
			{
				// Recv 완료 통지 처리
				session->recvQ.MoveRear(cbTransferred);
				CompleteRecvPacket(session);
				RecvPost(session);
			}
			else if (&session->sendOverlapped == overlapped)
			{
				// Send 완료 통지 처리
				CompleteSendPacket(session);
				InterlockedExchange((LONG*)&session->sendFlag, FALSE);
				if (session->sendQ.GetUseSize() > 0)
					SendPost(session);
			}
			else
			{
				// 다른 스레드에서 WorkerThread 에 보낸 메시지 처리
				MessageProc(cbTransferred, (WPARAM)session, (LPARAM)overlapped);
				continue;
			}
		}

		//--------------------------------------------------------------------
		// ioCount가 0이라는 것은 서버에서 세션 정리를 유도한 것. 세션을 정리한다.
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
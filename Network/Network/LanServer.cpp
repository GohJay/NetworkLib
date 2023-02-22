#include "LanServer.h"
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

LanServer::LanServer() : _sessionCnt(0), _sessionKey(0), _lastTimeoutProc(0)
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
bool LanServer::Start(const wchar_t* ipaddress, int port, int workerCreateCnt, int workerRunningCnt, WORD sessionMax, int timeoutSec, bool nagle)
{
	_workerCreateCnt = workerCreateCnt;
	_workerRunningCnt = workerRunningCnt;
	_sessionMax = sessionMax;
	_timeoutSec = timeoutSec;
	_stopSignal = false;
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
void LanServer::Stop()
{
	//--------------------------------------------------------------------
	// AcceptThread, ManagementThread 종료 신호 보내기
	//--------------------------------------------------------------------
	closesocket(_listenSocket);
	_stopSignal = true;

	//--------------------------------------------------------------------
	// AcceptThread, ManagementThread 종료 대기
	//--------------------------------------------------------------------
	HANDLE hHandle[2] = { _hAcceptThread, _hManagementThread };
	DWORD ret;
	ret = WaitForMultipleObjects(2, hHandle, TRUE, INFINITE);
	if (ret == WAIT_FAILED)
		OnError(NET_ERROR_RELEASE_FAILED, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());

	//--------------------------------------------------------------------
	// 남아있는 모든 세션 정리
	//--------------------------------------------------------------------
	SESSION* session;
	for (int index = 0; index < _sessionMax; index++)
	{
		session = &_sessionArray[index];
		if (session->releaseFlag != TRUE)
			DisconnectSession(session);
	}

	//--------------------------------------------------------------------
	// 모든 세션 정리 대기
	//--------------------------------------------------------------------
	while (_sessionCnt > 0)
		Sleep(500);

	//--------------------------------------------------------------------
	// WorkerThread 종료 신호 보내기
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);

	//--------------------------------------------------------------------
	// WorkerThread 종료 대기
	//--------------------------------------------------------------------
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
	if (session != nullptr)
	{
		DisconnectSession(session);
		CloseSession(session);
		return true;
	}
	return false;
}
bool LanServer::SendPacket(DWORD64 sessionID, NetPacket* packet)
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
int LanServer::GetSessionCount()
{
	return _sessionCnt;
}
int LanServer::GetUsePacketCount()
{
	return NetPacket::_packetPool.GetUseCount();
}
__int64 LanServer::GetTotalAcceptCount()
{
	return _monitoring.acceptTotal;
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
SESSION* LanServer::CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr)
{
	//--------------------------------------------------------------------
	// 세션 인덱스 배정
	//--------------------------------------------------------------------
	WORD index;
	if (!_indexStack.Pop(index))
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, NULL, NULL);
		return nullptr;
	}

	//--------------------------------------------------------------------
	// 세션 할당
	//--------------------------------------------------------------------
	SESSION* session = &_sessionArray[index];
	IncrementIOCount(session);
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
void LanServer::ReleaseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// 세션의 IOCount, releaseFlag 가 모두 0 인지 확인
	//--------------------------------------------------------------------
	if (InterlockedCompareExchange(&session->release, TRUE, FALSE) != FALSE)
		return;

	//--------------------------------------------------------------------
	// 세션 자원 정리
	//--------------------------------------------------------------------
	ClearSendPacket(session);
	closesocket(session->socket);

	//--------------------------------------------------------------------
	// 컨텐츠 부에 알림
	//--------------------------------------------------------------------
	OnClientLeave(session->sessionID);

	//--------------------------------------------------------------------
	// 세션 인덱스 반환
	//--------------------------------------------------------------------
	WORD index = GET_SESSION_INDEX(session->sessionID);
	_indexStack.Push(index);

	InterlockedDecrement16((SHORT*)&_sessionCnt);
}
void LanServer::DisconnectSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// 소켓에 현재 요청되어있는 모든 IO 를 중단
	//--------------------------------------------------------------------
	if (InterlockedExchange8(&session->disconnectFlag, TRUE) == FALSE)
		CancelIoEx((HANDLE)session->socket, NULL);
}
SESSION* LanServer::DuplicateSession(DWORD64 sessionID)
{
	WORD index = GET_SESSION_INDEX(sessionID);
	SESSION* session = &_sessionArray[index];
	IncrementIOCount(session);

	do
	{
		//--------------------------------------------------------------------
		// 릴리즈된 세션인지 확인
		//--------------------------------------------------------------------
		if (session->releaseFlag == TRUE)
			break;

		//--------------------------------------------------------------------
		// 재사용된 세션인지 확인
		//--------------------------------------------------------------------
		if (session->sessionID != sessionID)
			break;

		//--------------------------------------------------------------------
		// 찾던 세션 획득
		//--------------------------------------------------------------------
		return session;
	} while (0);

	CloseSession(session);
	return nullptr;
}
void LanServer::IncrementIOCount(SESSION* session)
{
	//--------------------------------------------------------------------
	// IO Count 증감
	//--------------------------------------------------------------------
	InterlockedIncrement16(&session->ioCount);
}
void LanServer::CloseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// IO Count 차감 값이 0일 경우 세션 릴리즈 요청
	//--------------------------------------------------------------------
	if (InterlockedDecrement16(&session->ioCount) == 0)
		QueueUserMessage(UM_POST_SESSION_RELEASE, session);
}
void LanServer::RecvPost(SESSION* session)
{
	//--------------------------------------------------------------------
	// Disconnect Flag 가 켜져있을 경우 return
	//--------------------------------------------------------------------
	if (session->disconnectFlag == TRUE)
		return;

	//--------------------------------------------------------------------
	// 수신용 링버퍼의 사이즈 구하기	
	//--------------------------------------------------------------------
	int directSize = session->recvQ.DirectEnqueueSize();
	if (directSize <= 0)
	{
		// 여유공간이 전혀 없다는 것은 링버퍼에 담긴 메시지에 컨텐츠 부에서 파싱 할 수 없는 오류가 있는 것. 연결을 끊는다.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		DisconnectSession(session);
		return;
	}

	//--------------------------------------------------------------------
	// WSARecv 를 위한 매개변수 초기화
	//--------------------------------------------------------------------
	ZeroMemory(&session->recvOverlapped, sizeof(OVERLAPPED));
	WSABUF wsaRecvBuf[2];
	wsaRecvBuf[0].buf = session->recvQ.GetRearBufferPtr();
	wsaRecvBuf[0].len = directSize;
	wsaRecvBuf[1].len = session->recvQ.GetFreeSize() - directSize;
	wsaRecvBuf[1].buf = session->recvQ.GetBufferPtr();

	//--------------------------------------------------------------------
	// WSARecv 처리
	//--------------------------------------------------------------------
	IncrementIOCount(session);

	DWORD flag = 0;
	int ret = WSARecv(session->socket, wsaRecvBuf, 2, NULL, &flag, &session->recvOverlapped, NULL);
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

			CloseSession(session);
			return;
		}

		//--------------------------------------------------------------------
		// WSARecv 작업 중단 여부 판단
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			CancelIoEx((HANDLE)session->socket, &session->recvOverlapped);
	}
}
void LanServer::SendPost(SESSION* session)
{
	for (;;)
	{
		//--------------------------------------------------------------------
		// Disconnect Flag 가 켜져있을 경우 return
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			return;

		//--------------------------------------------------------------------
		// 이미 전송 중인 Send 건이 있으면 return
		//--------------------------------------------------------------------
		if (InterlockedExchange8(&session->sendFlag, TRUE) == TRUE)
			return;

		if (session->sendQ.size() > 0)
			break;

		//--------------------------------------------------------------------
		// 전송할 데이터가 있는지 재차 확인 후 없으면 return
		//--------------------------------------------------------------------
		InterlockedExchange8(&session->sendFlag, FALSE);
		if (session->sendQ.size() <= 0)
			return;
	}

	//--------------------------------------------------------------------
	// WSASend 를 위한 매개변수 초기화
	//--------------------------------------------------------------------
	ZeroMemory(&session->sendOverlapped, sizeof(OVERLAPPED));
	WSABUF wsaBuf[MAX_SENDBUF];

	//--------------------------------------------------------------------
	// 송신할 직렬화 버퍼 포인터 Dequeue
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
	// WSASend 처리
	//--------------------------------------------------------------------
	IncrementIOCount(session);

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

			CloseSession(session);
			return;
		}

		//--------------------------------------------------------------------
		// WSASend 작업 중단 여부 판단
		//--------------------------------------------------------------------
		if (session->disconnectFlag == TRUE)
			CancelIoEx((HANDLE)session->socket, &session->sendOverlapped);
	}
}
void LanServer::RecvRoutine(SESSION* session, DWORD cbTransferred)
{
	session->lastRecvTime = timeGetTime();
	session->recvQ.MoveRear(cbTransferred);
	for (;;)
	{
		int ret = CompleteRecvPacket(session);
		if (ret == 1)
			break;
		else if (ret == -1)
		{
			DisconnectSession(session);
			break;
		}
	}
	RecvPost(session);
}
void LanServer::SendRoutine(SESSION* session, DWORD cbTransferred)
{
	CompleteSendPacket(session);
	InterlockedExchange8(&session->sendFlag, FALSE);
	if (session->sendQ.size() > 0)
		SendPost(session);
}
int LanServer::CompleteRecvPacket(SESSION* session)
{
	//--------------------------------------------------------------------
	// 수신용 링버퍼의 사이즈가 Header 크기보다 큰지 확인
	//--------------------------------------------------------------------
	LAN_PACKET_HEADER header;
	int size = session->recvQ.GetUseSize();
	if (size <= sizeof(header))
		return 1;

	//--------------------------------------------------------------------
	// 수신용 링버퍼에서 Header 를 Peek 하여 확인
	//--------------------------------------------------------------------
	int ret = session->recvQ.Peek((char*)&header, sizeof(header));
	if (ret != sizeof(header))
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, peek size: %d, ret size: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE, sizeof(header), ret);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		return -1;
	}

	//--------------------------------------------------------------------
	// Payload 크기가 설정해둔 최대 크기를 초과하는지 확인
	//--------------------------------------------------------------------
	if (header.len > MAX_PAYLOAD)
		return -1;

	//--------------------------------------------------------------------
	// 수신용 링버퍼의 사이즈가 Header + Payload 크기 만큼 있는지 확인
	//--------------------------------------------------------------------
	if (session->recvQ.GetUseSize() < sizeof(header) + header.len)
		return 1;

	session->recvQ.MoveFront(sizeof(header));

	//--------------------------------------------------------------------
	// 직렬화 버퍼에 Header 담기
	//--------------------------------------------------------------------
	NetPacket* packet = NetPacket::Alloc();
	packet->PutHeader((char*)&header, sizeof(header));

	//--------------------------------------------------------------------
	// 직렬화 버퍼에 Payload 담기
	//--------------------------------------------------------------------
	ret = session->recvQ.Dequeue(packet->GetRearBufferPtr(), header.len);
	if (ret != header.len)
	{
		Logger::WriteLog(L"Net", LOG_LEVEL_ERROR, L"%s() line: %d - error: %d, dequeue size: %d, ret size: %d", __FUNCTIONW__, __LINE__, NET_FATAL_INVALID_SIZE, header.len, ret);
		OnError(NET_FATAL_INVALID_SIZE, __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		NetPacket::Free(packet);
		return -1;
	}
	packet->MoveRear(ret);

	try
	{
		//--------------------------------------------------------------------
		// 컨텐츠 부에 직렬화 버퍼 전달
		//--------------------------------------------------------------------
		OnRecv(session->sessionID, packet);
	}
	catch (NetException& ex)
	{
		OnError(ex.GetLastError(), __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		NetPacket::Free(packet);
		return -1;
	}

	NetPacket::Free(packet);
	InterlockedIncrement(&_monitoring.curTPS.recv);
	return 0;
}
void LanServer::CompleteSendPacket(SESSION* session)
{
	//--------------------------------------------------------------------
	// 전송 완료한 직렬화 버퍼 정리
	//--------------------------------------------------------------------
	NetPacket* packet;
	for (int count = 0; count < session->sendBufCount; count++)
	{
		packet = session->sendBuf[count];
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
		// 여유공간이 부족하여 메시지를 담을 수 없다는 것은 서버가 처리해줄 수 있는 한계를 지났다는 것. 연결을 끊는다.
		OnError(NET_ERROR_NETBUFFER_OVER, __FUNCTIONW__, __LINE__, session->sessionID, size);
		DisconnectSession(session);
		return;
	}

	//--------------------------------------------------------------------
	// 직렬화 버퍼에 네트워크 부 헤더 담기
	//--------------------------------------------------------------------
	LAN_PACKET_HEADER header;
	header.len = packet->GetUseSize();
	packet->PutHeader((char*)&header, sizeof(header));

	//--------------------------------------------------------------------
	// 송신용 큐에 직렬화 버퍼 담기
	//--------------------------------------------------------------------
	packet->IncrementRefCount();
	session->sendQ.Enqueue(packet);

	//--------------------------------------------------------------------
	// 송신 요청
	//--------------------------------------------------------------------
	IncrementIOCount(session);
	QueueUserMessage(UM_POST_SEND_PACKET, (LPVOID)session);
}
void LanServer::ClearSendPacket(SESSION* session)
{
	NetPacket* packet;
	for (int count = 0; count < session->sendBufCount; count++)
	{
		//--------------------------------------------------------------------
		// 전송 대기중이던 직렬화 버퍼 정리
		//--------------------------------------------------------------------
		packet = session->sendBuf[count];
		NetPacket::Free(packet);
	}
	session->sendBufCount = 0;

	while (session->sendQ.size() > 0)
	{
		//--------------------------------------------------------------------
		// 송신용 큐에 남아있는 직렬화 버퍼 정리
		//--------------------------------------------------------------------
		session->sendQ.Dequeue(packet);
		NetPacket::Free(packet);
	}
}
void LanServer::QueueUserMessage(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// WorkerThread 에게 Job 요청
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, message, (ULONG_PTR)lpParam, NULL);
}
void LanServer::UserMessageProc(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// 다른 스레드로부터 요청받은 Job 처리
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
	case UM_POST_SESSION_RELEASE:
		{
			SESSION* session = (SESSION*)lpParam;
			ReleaseSession(session);
		}
		break;
	default:
		break;
	}
}
void LanServer::TimeoutProc()
{
	//--------------------------------------------------------------------
	// 타임아웃 사용 옵션이 OFF일 경우 return
	//--------------------------------------------------------------------
	if (_timeoutSec <= 0)
		return;

	//--------------------------------------------------------------------
	// 타임아웃 처리 시간이 아직 아닐 경우 return
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
		// 릴리즈된 세션인지 확인
		//--------------------------------------------------------------------
		session = &_sessionArray[index];
		if (session->releaseFlag == TRUE)
			continue;

		//--------------------------------------------------------------------
		// 타임아웃 여부 판단
		//--------------------------------------------------------------------
		sessionID = session->sessionID;
		if (session->lastRecvTime > timeout)
			continue;

		IncrementIOCount(session);

		do
		{
			//--------------------------------------------------------------------
			// 릴리즈된 세션인지 확인
			//--------------------------------------------------------------------
			if (session->releaseFlag == TRUE)
				break;

			//--------------------------------------------------------------------
			// 재사용된 세션인지 확인
			//--------------------------------------------------------------------
			if (session->sessionID != sessionID)
				break;

			//--------------------------------------------------------------------
			// 타임아웃 처리
			//--------------------------------------------------------------------
			DisconnectSession(session);
		} while (0);

		CloseSession(session);
	}

	_lastTimeoutProc = currentTime;
}
void LanServer::UpdateTPS()
{
	//--------------------------------------------------------------------
	// 모니터링용 TPS 갱신
	//--------------------------------------------------------------------
	_monitoring.oldTPS.accept = InterlockedExchange(&_monitoring.curTPS.accept, 0);
	_monitoring.oldTPS.recv = InterlockedExchange(&_monitoring.curTPS.recv, 0);
	_monitoring.oldTPS.send = InterlockedExchange(&_monitoring.curTPS.send, 0);
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

	_hWorkerThread = new HANDLE[_workerCreateCnt];
	_sessionArray = (SESSION*)_aligned_malloc(sizeof(SESSION) * _sessionMax, 64);

	int index;
	for (index = 0; index < _sessionMax; index++)
		new(&_sessionArray[index]) SESSION();

	for (index = _sessionMax - 1; index >= 0; index--)
		_indexStack.Push(index);

	return true;
}
void LanServer::Release()
{
	WORD index;
	while (_indexStack.size() > 0)
		_indexStack.Pop(index);

	for (index = 0; index < _sessionMax; index++)
		_sessionArray[index].~SESSION();

	CloseHandle(_hCompletionPort);
	CloseHandle(_hManagementThread);
	CloseHandle(_hAcceptThread);
	for (int i = 0; i < _workerCreateCnt; i++)
		CloseHandle(_hWorkerThread[i]);

	_aligned_free(_sessionArray);
	delete[] _hWorkerThread;
}
unsigned int LanServer::AcceptThread()
{
	for (;;)
	{
		SOCKADDR_IN socketAddr;
		int socketAddrSize = sizeof(socketAddr);
		ZeroMemory(&socketAddr, socketAddrSize);

		//--------------------------------------------------------------------
		// Accept 처리. 처리할 것이 없다면 Block
		//--------------------------------------------------------------------
		SOCKET socket = accept(_listenSocket, (SOCKADDR*)&socketAddr, &socketAddrSize);
		if (socket == INVALID_SOCKET)
			break;

		//--------------------------------------------------------------------
		// 동시접속자 수 확인. 최대치를 초과할 경우 연결 종료
		//--------------------------------------------------------------------
		if (_sessionCnt >= _sessionMax)
		{
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// 컨텐츠 부에 신규 접속자의 IP, Port 를 전달하여 접속을 허용할 것인지 확인
		//--------------------------------------------------------------------
		wchar_t ip[16];
		int port;
		InetNtop(AF_INET, &socketAddr.sin_addr, ip, sizeof(ip) / 2);
		port = ntohs(socketAddr.sin_port);
		if (!OnConnectionRequest(ip, port))
		{
			// 컨텐츠 부에서 접속을 허용하지 않는다면 연결을 끊는다.
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// 신규 접속자의 세션 할당
		//--------------------------------------------------------------------
		SESSION* session = CreateSession(socket, &socketAddr);
		if (session == nullptr)
		{
			closesocket(socket);
			continue;
		}

		//--------------------------------------------------------------------
		// 할당된 세션을 IOCP에 등록
		//--------------------------------------------------------------------
		CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)session, NULL);

		//--------------------------------------------------------------------
		// 신규 접속자의 정보를 컨텐츠 부에 알리고 수신 등록
		//--------------------------------------------------------------------
		OnClientJoin(session->sessionID);
		RecvPost(session);

		//--------------------------------------------------------------------
		// AcceptThread 에서 참조하던 세션 반환
		//--------------------------------------------------------------------
		CloseSession(session);

		_monitoring.acceptTotal++;
		InterlockedIncrement(&_monitoring.curTPS.accept);
	}
	return 0;
}
unsigned int LanServer::WorkerThread()
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
				// 종료 신호가 오면 스레드를 종료
				PostQueuedCompletionStatus(_hCompletionPort, 0, NULL, NULL);
				break;
			}
			else
			{
				// 다른 스레드에게 받은 비동기 메시지 처리
				UserMessageProc(cbTransferred, (LPVOID)session);
				continue;
			}
		}

		if (cbTransferred != 0)
		{
			if (&session->recvOverlapped == overlapped)
			{
				// Recv 완료 통지 처리
				RecvRoutine(session, cbTransferred);
			}
			else if (&session->sendOverlapped == overlapped)
			{
				// Send 완료 통지 처리
				SendRoutine(session, cbTransferred);
			}
		}

		CloseSession(session);
	}
	return 0;
}
unsigned int LanServer::ManagementThread()
{
	while (!_stopSignal)
	{
		// TPS 갱신
		UpdateTPS();

		// 타임아웃 처리
		TimeoutProc();

		// 1000ms 딜레이
		Sleep(1000);
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
unsigned int __stdcall LanServer::WrapManagementThread(LPVOID lpParam)
{
	LanServer* server = (LanServer*)lpParam;
	return server->ManagementThread();
}
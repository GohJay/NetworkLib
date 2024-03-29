#include "LanClient.h"
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

LanClient::LanClient()
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
LanClient::~LanClient()
{
	//--------------------------------------------------------------------
	// 남아있는 세션 정리
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
		DisconnectSession(&_session);

	//--------------------------------------------------------------------
	// 세션 정리 대기
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
bool LanClient::Connect(const wchar_t* ipaddress, int port, bool nagle)
{
	//--------------------------------------------------------------------
	// 이미 연결된 정보가 있는지 확인
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
	{
		OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, NULL);
		return false;
	}

	SOCKADDR_IN sockAddr;
	ZeroMemory(&sockAddr, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(port);
	InetPton(AF_INET, ipaddress, &sockAddr.sin_addr);

	//--------------------------------------------------------------------
	// 연결에 사용할 소켓 할당
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
	// 서버에 연결
	//--------------------------------------------------------------------
	ret = connect(sock, (SOCKADDR*)&sockAddr, sizeof(sockAddr));
	if (ret == SOCKET_ERROR)
	{
		OnError(NET_ERROR_CONNECT_ERROR, __FUNCTIONW__, __LINE__, NULL, WSAGetLastError());
		closesocket(sock);
		return false;
	}

	//--------------------------------------------------------------------
	// 세션 할당
	//--------------------------------------------------------------------
	SESSION* session = CreateSession(sock, &sockAddr);

	//--------------------------------------------------------------------
	// 할당된 세션을 IOCP에 등록
	//--------------------------------------------------------------------
	CreateIoCompletionPort((HANDLE)session->socket, _hCompletionPort, (ULONG_PTR)session, NULL);

	//--------------------------------------------------------------------
	// 연결 정보를 컨텐츠 부에 알리고 수신 등록
	//--------------------------------------------------------------------
	OnEnterJoinServer();
	RecvPost(session);

	//--------------------------------------------------------------------
	// 해당 스레드에서 참조하던 세션 반환
	//--------------------------------------------------------------------
	CloseSession(session);
	return true;
}
bool LanClient::Disconnect()
{
	SESSION* session = DuplicateSession();
	if (session != nullptr)
	{
		DisconnectSession(session);
		CloseSession(session);
		return true;
	}
	return false;
}
bool LanClient::SendPacket(NetPacket* packet)
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
SESSION* LanClient::CreateSession(SOCKET socket, SOCKADDR_IN* socketAddr)
{
	//--------------------------------------------------------------------
	// 세션 할당
	//--------------------------------------------------------------------
	IncrementIOCount(&_session);
	memmove(&_session.socketAddr, socketAddr, sizeof(SOCKADDR_IN));
	_session.socket = socket;
	_session.recvQ.ClearBuffer();
	_session.sendBufCount = 0;
	_session.sendFlag = FALSE;
	_session.disconnectFlag = FALSE;
	_session.releaseFlag = FALSE;
	return &_session;
}
void LanClient::ReleaseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// 세션의 IOCount, releaseFlag 가 모두 0 인지 확인
	//--------------------------------------------------------------------
	if (InterlockedCompareExchange(&session->status, TRUE, FALSE) != FALSE)
		return;

	//--------------------------------------------------------------------
	// 세션 자원 정리
	//--------------------------------------------------------------------
	ClearSendPacket(session);
	closesocket(session->socket);

	//--------------------------------------------------------------------
	// 컨텐츠 부에 알림
	//--------------------------------------------------------------------
	QueueUserMessage(UM_ALERT_SERVER_LEAVE, NULL);
}
void LanClient::DisconnectSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// 소켓에 현재 요청되어있는 모든 IO 를 중단
	//--------------------------------------------------------------------
	if (InterlockedExchange8(&session->disconnectFlag, TRUE) == FALSE)
		CancelIoEx((HANDLE)session->socket, NULL);
}
SESSION* LanClient::DuplicateSession()
{
	IncrementIOCount(&_session);

	//--------------------------------------------------------------------
	// 릴리즈된 세션인지 확인
	//--------------------------------------------------------------------
	if (_session.releaseFlag != TRUE)
	{
		// 찾던 세션 획득
		return &_session;
	}

	CloseSession(&_session);
	return nullptr;
}
void LanClient::IncrementIOCount(SESSION* session)
{
	//--------------------------------------------------------------------
	// IO Count 증감
	//--------------------------------------------------------------------
	InterlockedIncrement16(&session->ioCount);
}
void LanClient::CloseSession(SESSION* session)
{
	//--------------------------------------------------------------------
	// IO Count 차감 값이 0일 경우 세션 릴리즈 처리
	//--------------------------------------------------------------------
	if (InterlockedDecrement16(&session->ioCount) == 0)
		ReleaseSession(session);
}
void LanClient::RecvPost(SESSION* session)
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

			DisconnectSession(session);
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
void LanClient::SendPost(SESSION* session)
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

			DisconnectSession(session);
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
void LanClient::RecvRoutine(SESSION* session, DWORD cbTransferred)
{
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
void LanClient::SendRoutine(SESSION* session, DWORD cbTransferred)
{
	CompleteSendPacket(session);
	InterlockedExchange8(&session->sendFlag, FALSE);
	if (session->sendQ.size() > 0)
		SendPost(session);
}
int LanClient::CompleteRecvPacket(SESSION* session)
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
		OnRecv(packet);
	}
	catch (NetException& ex)
	{
		OnError(ex.GetLastError(), __FUNCTIONW__, __LINE__, session->sessionID, NULL);
		NetPacket::Free(packet);
		return -1;
	}

	NetPacket::Free(packet);
	return 0;
}
void LanClient::CompleteSendPacket(SESSION* session)
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

	session->sendBufCount = 0;
}
void LanClient::TrySendPacket(SESSION* session, NetPacket* packet)
{
	int size = session->sendQ.size();
	if (size >= MAX_SENDBUF)
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
void LanClient::ClearSendPacket(SESSION* session)
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
void LanClient::QueueUserMessage(DWORD message, LPVOID lpParam)
{
	//--------------------------------------------------------------------
	// WorkerThread 에게 Job 요청
	//--------------------------------------------------------------------
	PostQueuedCompletionStatus(_hCompletionPort, message, (ULONG_PTR)lpParam, NULL);
}
void LanClient::UserMessageProc(DWORD message, LPVOID lpParam)
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
	case UM_ALERT_SERVER_LEAVE:
		{
			OnLeaveServer();
		}
		break;
	default:
		break;
	}
}
unsigned int LanClient::WorkerThread()
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
unsigned int __stdcall LanClient::WrapWorkerThread(LPVOID lpParam)
{
	LanClient* client = (LanClient*)lpParam;
	return client->WorkerThread();
}

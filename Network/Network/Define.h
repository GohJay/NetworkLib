#ifndef __DEFINE__H
#define __DEFINE__H
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "NetPacket.h"

#define SESSIONID_INDEX_MASK            0x000000000000FFFF
#define SESSIONID_KEY_MASK              0xFFFFFFFFFFFF0000

#define GET_SESSION_INDEX(sessionid)	((sessionid) & SESSIONID_INDEX_MASK)
#define MAKE_SESSIONID(key, index)      ((key) << 16) | (index)

#define MAX_SENDBUF                     512
#define MAX_PAYLOAD                     1000
#define MAX_CONTENT                     20

enum SESSION_JOB_TYPE
{
	JOB_TYPE_PACKET_RECV = 0,
	JOB_TYPE_SESSION_MOVE,
};

struct SESSION_JOB
{
	SESSION_JOB_TYPE type;
	DWORD64 sessionID;
	WORD destContentID;
	Jay::NetPacket* packet;
};

struct alignas(64) SESSION
{
	union {
		struct {
			BOOL releaseFlag;
			LONG ioCount;
		};
		LONGLONG release;
	};
	OVERLAPPED* recvOverlapped;
	OVERLAPPED* sendOverlapped;
	DWORD64 sessionID;
	SOCKET socket;
	WCHAR ip[16];
	USHORT port;
	DWORD lastRecvTime;
	BOOL disconnectFlag;
	BOOL sendFlag;
	LONG sendBufCount;
	Jay::NetPacket** sendBuf;
	Jay::RingBuffer recvQ;
	Jay::LockFreeQueue<Jay::NetPacket*> sendQ;
	Jay::LockFreeQueue<SESSION_JOB*> jobQ;

	SESSION() {
		release = TRUE;
		recvOverlapped = (OVERLAPPED*)malloc(sizeof(OVERLAPPED));
		sendOverlapped = (OVERLAPPED*)malloc(sizeof(OVERLAPPED));
		sendBuf = (Jay::NetPacket**)malloc(sizeof(void*) * MAX_SENDBUF);
	}
	~SESSION() {
		free(recvOverlapped);
		free(sendOverlapped);
		free(sendBuf);
	}
};

struct TPS
{
	LONG accept;
	LONG recv;
	LONG send;
};

struct MONITORING
{
	TPS oldTPS;
	TPS curTPS;
	LONGLONG acceptTotal;
};

#endif

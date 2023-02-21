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
			SHORT releaseFlag;
			SHORT ioCount;
		};
		LONG release;
	};
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	SOCKADDR_IN socketAddr;
	SOCKET socket;
	DWORD64 sessionID;
	DWORD lastRecvTime;
	CHAR disconnectFlag;
	CHAR sendFlag;
	SHORT sendBufCount;
	Jay::NetPacket** sendBuf;
	Jay::RingBuffer recvQ;
	Jay::LockFreeQueue<Jay::NetPacket*> sendQ;
	Jay::LockFreeQueue<SESSION_JOB*> jobQ;

	SESSION() {
		release = TRUE;
		sendBuf = (Jay::NetPacket**)malloc(sizeof(void*) * MAX_SENDBUF);
	}
	~SESSION() {
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
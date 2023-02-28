#ifndef __SESSION__H
#define __SESSION__H
#include "Define.h"
#include "RingBuffer.h"
#include "NetPacket.h"
#include "LFQueue.h"

#define SESSIONID_INDEX_MASK            0x000000000000FFFF
#define SESSIONID_KEY_MASK              0xFFFFFFFFFFFF0000

#define GET_SESSION_INDEX(sessionid)	((sessionid) & SESSIONID_INDEX_MASK)
#define MAKE_SESSIONID(key, index)      ((key) << 16) | (index)

enum SESSION_JOB_TYPE
{
	JOB_TYPE_PACKET_RECV = 0,
	JOB_TYPE_SESSION_MOVE,
};

struct SESSION_JOB
{
	SESSION_JOB_TYPE type;
	DWORD64 sessionID;
	LPVOID lpParam1;
	LPVOID lpParam2;
	LPVOID lpParam3;
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
	Jay::LFQueue<Jay::NetPacket*> sendQ;
	Jay::LFQueue<SESSION_JOB*> jobQ;

	SESSION() {
		release = TRUE;
		sendBuf = (Jay::NetPacket**)malloc(sizeof(void*) * MAX_SENDBUF);
	}
	~SESSION() {
		free(sendBuf);
	}
};

#endif

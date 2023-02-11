#ifndef __DEFINE__H
#define __DEFINE__H
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "NetPacket.h"

#define SESSIONID_INDEX_MASK			0x000000000000FFFF
#define SESSIONID_KEY_MASK				0xFFFFFFFFFFFF0000

#define GET_SESSION_INDEX(sessionid)	((sessionid) & SESSIONID_INDEX_MASK)
#define MAKE_SESSIONID(key, index)		((key) << 16) | (index)

#define MAX_SENDBUF						500
#define MAX_PAYLOAD						1000

struct SESSION
{
	union {
		struct {
			BOOL releaseFlag;
			LONG ioCount;
		};
		LONGLONG release = TRUE;
	};
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	DWORD64 sessionID;
	SOCKET socket;
	WCHAR ip[16];
	INT port;
	DWORD lastRecvTime;
	Jay::RingBuffer recvQ;
	Jay::LockFreeQueue<Jay::NetPacket*> sendQ;
	Jay::NetPacket* sendBuf[MAX_SENDBUF];
	LONG sendBufCount;
	BOOL sendFlag;
	BOOL disconnectFlag;
};

struct TPS
{
	LONG accept;
	LONG recv;
	LONG send;
};

#endif

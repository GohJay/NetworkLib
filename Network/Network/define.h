#ifndef __DEFINE__H
#define __DEFINE__H
#include "RingBuffer.h"
#include "LockFreeQueue.h"
#include "NetPacket.h"

#define SESSIONID_INDEX_MASK			0x000000000000FFFF
#define SESSIONID_KEY_MASK				0xFFFFFFFFFFFF0000

#define GET_SESSION_INDEX(sessionid)	((sessionid) & SESSIONID_INDEX_MASK)
#define MAKE_SESSIONID(key, index)		((key) << 16) | (index)

#define MAX_SENDBUF						512
#define MAX_PAYLOAD						1000

struct TPS
{
	LONG accept;
	LONG recv;
	LONG send;
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
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	DWORD64 sessionID;
	SOCKET socket;
	WCHAR ip[16];
	USHORT port;
	DWORD lastRecvTime;
	Jay::RingBuffer recvQ;
	Jay::LockFreeQueue<Jay::NetPacket*> sendQ;
	Jay::NetPacket** sendBuf;
	LONG sendBufCount;
	BOOL sendFlag;
	BOOL disconnectFlag;
	
	SESSION() { release = TRUE, sendBuf = (Jay::NetPacket**)malloc(sizeof(void*) * MAX_SENDBUF); }
	~SESSION() { free(sendBuf); }
};

#endif

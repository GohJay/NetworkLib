#ifndef __DEFINE__H
#define __DEFINE__H
#include "RingBuffer.h"

struct SESSION
{
	SESSION() : sessionID(-1)
	{
		InitializeSRWLock(&lock);
	}
	OVERLAPPED recvOverlapped;
	OVERLAPPED sendOverlapped;
	SRWLOCK lock;
	DWORD64 sessionID;
	SOCKET socket;
	wchar_t ip[16];
	int port;
	LONG ioCount;
	Jay::RingBuffer recvQ;
	Jay::RingBuffer sendQ;
	BOOL sendFlag;
	int sendBufCount;
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
};

#endif

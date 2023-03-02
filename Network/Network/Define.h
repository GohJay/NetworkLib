#ifndef __DEFINE__H
#define __DEFINE__H

#define MAX_SENDBUF				512
#define MAX_PAYLOAD				500
#define MAX_CONTENT				20

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
	LONG64 acceptTotal;
};

#endif

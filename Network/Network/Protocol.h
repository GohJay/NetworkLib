#ifndef __PROTOCOL__H
#define __PROTOCOL__H

#define PACKET_CODE		0x77

#pragma pack(push, 1)

struct LAN_PACKET_HEADER
{
	WORD len;
};

struct NET_PACKET_HEADER
{
	BYTE code;
	WORD len;
	BYTE randKey;
	BYTE checkSum;
};

#pragma pack(pop)

#endif

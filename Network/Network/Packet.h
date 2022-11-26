#ifndef __PACKET__H_
#define __PACKET__H_
#include "Protocol.h"
#include "SerializationBuffer.h"

namespace Jay
{
	inline
		void MakeHeader(LAN_PACKET_HEADER* header, SerializationBuffer* packet)
	{
		header->len = packet->GetUseSize();
	}
}

#endif

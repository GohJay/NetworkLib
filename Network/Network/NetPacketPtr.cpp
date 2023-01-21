#include "NetPacketPtr.h"

using namespace Jay;

NetPacketPtr::NetPacketPtr()
{
	_packet = NetPacket::Alloc();
}
NetPacketPtr::NetPacketPtr(const NetPacketPtr& ref)
{
	_packet = ref._packet;
	_packet->IncrementRefCount();
}
NetPacketPtr& NetPacketPtr::operator=(const NetPacketPtr& ref)
{
	this->~NetPacketPtr();
	_packet = ref._packet;
	_packet->IncrementRefCount();
	return *this;
}
NetPacketPtr::~NetPacketPtr()
{
	NetPacket::Free(_packet);
}

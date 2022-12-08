#include "NetPacketPtr.h"

USEJAYNAMESPACE
NetPacketPtr::NetPacketPtr()
{
	_packet = NetPacket::Alloc();
	_packet->_refCount = 1;
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
	if (_packet->DecrementRefCount() == 0)
		NetPacket::Free(_packet);
}

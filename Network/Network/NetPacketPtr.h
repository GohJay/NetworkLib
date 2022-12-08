#ifndef __NETPACKETPTR__H_
#define __NETPACKETPTR__H_
#include "Base.h"
#include "NetPacket.h"

JAYNAMESPACE
class LanServer;
class NetPacketPtr
{
	/**
	* @file		NetPacketPtr.h
	* @brief	Network PacketPtr Class (SmartPointer)
	* @details	네트워크 송수신을 위한 직렬화버퍼의 스마트포인터 클래스
	* @author   고재현
	* @date		2022-12-03
	* @version  1.0.0
	**/
public:
	NetPacketPtr();
	NetPacketPtr(const NetPacketPtr& ref);
	NetPacketPtr& operator = (const NetPacketPtr& ref);
	~NetPacketPtr();
public:
	inline NetPacket* operator ->();

	inline NetPacketPtr& operator << (const char value);
	inline NetPacketPtr& operator << (const unsigned char value);

	inline NetPacketPtr& operator << (const short value);
	inline NetPacketPtr& operator << (const unsigned short value);

	inline NetPacketPtr& operator << (const long value);
	inline NetPacketPtr& operator << (const unsigned long value);

	inline NetPacketPtr& operator << (const long long value);
	inline NetPacketPtr& operator << (const unsigned long long value);

	inline NetPacketPtr& operator << (const int value);
	inline NetPacketPtr& operator << (const unsigned int value);

	inline NetPacketPtr& operator << (const float value);
	inline NetPacketPtr& operator << (const double value);

	inline NetPacketPtr& operator >> (char& value);
	inline NetPacketPtr& operator >> (unsigned char& value);

	inline NetPacketPtr& operator >> (short& value);
	inline NetPacketPtr& operator >> (unsigned short& value);

	inline NetPacketPtr& operator >> (long& value);
	inline NetPacketPtr& operator >> (unsigned long& value);

	inline NetPacketPtr& operator >> (long long& value);
	inline NetPacketPtr& operator >> (unsigned long long& value);

	inline NetPacketPtr& operator >> (int& value);
	inline NetPacketPtr& operator >> (unsigned int& value);

	inline NetPacketPtr& operator >> (float& value);
	inline NetPacketPtr& operator >> (double& value);
private:
	inline NetPacket* operator *();
private:
	NetPacket* _packet;
	friend class LanServer;
};

inline NetPacket* NetPacketPtr::operator->()
{
	return _packet;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const char value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const unsigned char value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const short value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const unsigned short value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const long value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const unsigned long value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const long long value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const unsigned long long value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const int value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const unsigned int value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const float value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator<<(const double value)
{
	(*_packet) << value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(char& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(unsigned char& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(short& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(unsigned short& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(long& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(unsigned long& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(long long& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(unsigned long long& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(int& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(unsigned int& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(float& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacketPtr& NetPacketPtr::operator>>(double& value)
{
	(*_packet) >> value;
	return *this;
}
inline NetPacket* NetPacketPtr::operator*()
{
	return _packet;
}
JAYNAMESPACEEND

#endif

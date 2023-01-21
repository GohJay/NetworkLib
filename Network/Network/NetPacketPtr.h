#ifndef __NETPACKETPTR__H_
#define __NETPACKETPTR__H_
#include "Base.h"
#include "NetPacket.h"

namespace Jay
{
	class NetServer;
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

		inline NetPacketPtr& operator << (const char value) throw(...);
		inline NetPacketPtr& operator << (const unsigned char value) throw(...);

		inline NetPacketPtr& operator << (const short value) throw(...);
		inline NetPacketPtr& operator << (const unsigned short value) throw(...);

		inline NetPacketPtr& operator << (const long value) throw(...);
		inline NetPacketPtr& operator << (const unsigned long value) throw(...);

		inline NetPacketPtr& operator << (const long long value) throw(...);
		inline NetPacketPtr& operator << (const unsigned long long value) throw(...);

		inline NetPacketPtr& operator << (const int value) throw(...);
		inline NetPacketPtr& operator << (const unsigned int value) throw(...);

		inline NetPacketPtr& operator << (const float value) throw(...);
		inline NetPacketPtr& operator << (const double value) throw(...);

		inline NetPacketPtr& operator >> (char& value) throw(...);
		inline NetPacketPtr& operator >> (unsigned char& value) throw(...);

		inline NetPacketPtr& operator >> (short& value) throw(...);
		inline NetPacketPtr& operator >> (unsigned short& value) throw(...);

		inline NetPacketPtr& operator >> (long& value) throw(...);
		inline NetPacketPtr& operator >> (unsigned long& value) throw(...);

		inline NetPacketPtr& operator >> (long long& value) throw(...);
		inline NetPacketPtr& operator >> (unsigned long long& value) throw(...);

		inline NetPacketPtr& operator >> (int& value) throw(...);
		inline NetPacketPtr& operator >> (unsigned int& value) throw(...);

		inline NetPacketPtr& operator >> (float& value) throw(...);
		inline NetPacketPtr& operator >> (double& value) throw(...);

		inline NetPacket* operator *();
	private:
		NetPacket* _packet;
		friend class NetServer;
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
}

#endif

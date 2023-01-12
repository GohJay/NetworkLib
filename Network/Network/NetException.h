#ifndef __NET_EXCEPTION__H_
#define __NET_EXCEPTION__H_
#include "Exception.h"

namespace Jay
{
	class NetException : public Exception
	{
	public:
		NetException(int error) : Exception(error)
		{
		}
		~NetException()
		{
		}
	public:
		virtual const char* what() const
		{
			return "Network Exception";
		}
	};
}

#endif

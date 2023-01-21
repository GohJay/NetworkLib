#ifndef __NET_EXCEPTION__H_
#define __NET_EXCEPTION__H_
#include <exception>

namespace Jay
{
	class NetException : public std::exception
	{
	public:
		NetException(int error) : _error(error)
		{
		}
		~NetException()
		{
		}
	public:
		int GetLastError()
		{
			return _error;
		}
		virtual const char* what() const
		{
			return "Network Exception";
		}
	private:
		int _error;
	};
}

#endif

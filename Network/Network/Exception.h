#ifndef __EXCEPTION__H_
#define __EXCEPTION__H_
#include <exception>

namespace Jay
{
	class Exception : public std::exception
	{
	public:
		Exception(int error) : _error(error) {}
		virtual ~Exception() {}
	public:
		int GetLastError() { return _error; }
		virtual const char* what() const = 0;
	private:
		int _error;
	};
}

#endif

#ifndef __LOG__H_
#define __LOG__H_
#include "Base.h"

JAYNAMESPACE
/**
* @file		Log.h
* @brief	Debug Log Global Function
* @details	디버깅 테스트를 위한 로깅 함수
* @author   고재현
* @date		2022-06-01
* @version  1.0.1
**/
inline
	void WriteLog(const char * args, ...)
{
	char _log[512];
	*_log = '\0';
	va_list _args;
	va_start(_args, args);
	vsnprintf(_log, sizeof(_log), args, _args);
	va_end(_args);
	OutputDebugStringA(_log);
}
inline
	void WriteLog(const wchar_t * args, ...)
{
	wchar_t _log[512];
	*_log = L'\0';
	va_list _args;
	va_start(_args, args);	
	_vsnwprintf(_log, sizeof(_log) / sizeof(_log[0]), args, _args);
	va_end(_args);
	OutputDebugStringW(_log);
}
JAYNAMESPACEEND

#endif !__LOG__H_

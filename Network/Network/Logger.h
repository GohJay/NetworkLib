#ifndef __LOG__H_
#define __LOG__H_
#include "Lock.h"
#include <Windows.h>

#define LOG_LEVEL_DEBUG		0
#define LOG_LEVEL_SYSTEM	1
#define LOG_LEVEL_ERROR		2

namespace Jay
{
	class Logger
	{
		/**
		* @file		Logger.h
		* @brief	File Logger Class
		* @details	파일 로그 출력용 클래스
		* @author	고재현
		* @date		2022-02-11
		* @version	1.0.3
		**/
	private:
		Logger();
		~Logger();
	public:
		static void SetLogLevel(int logLevel);
		static void SetLogPath(const wchar_t* logPath);
		static void WriteLog(const wchar_t* type, int logLevel, const wchar_t* fmt, ...);
		static void WriteHex(const wchar_t* type, int logLevel, const wchar_t* log, BYTE* byte, int byteLen);
	private:
		static void WriteProc(const wchar_t* type, int logLevel, const wchar_t* buffer, bool truncated);
	private:
		static DWORD _logIndex;
		static int _logLevel;
		static wchar_t _logPath[MAX_PATH];
		static SRWLock _logLock;
		static Logger _instance;
	};
}

#endif //__LOG__H_

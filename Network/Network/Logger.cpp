#include "Logger.h"
#include "FileUtil.h"
#include <locale.h>
#include <wchar.h>
#include <strsafe.h>
#include <time.h>

#define LOG_LEVEL_DEBUGW			L"DEBUG"
#define LOG_LEVEL_SYSTEMW			L"SYSTEM"
#define LOG_LEVEL_ERRORW			L"ERROR"
#define LOG_LEVEL_UNKNOWNW			L"UNKNOWN"
#define LOG_ERROR_BUFFER_TRUNCATED	L"!!! Buffer Truncated !!!"

using namespace Jay;

DWORD Logger::_logIndex;
int Logger::_logLevel;
wchar_t Logger::_logPath[MAX_PATH];
SRWLock Logger::_logLock;
Logger Logger::_instance;

Logger::Logger()
{
	//--------------------------------------------------------------------
	// Initial
	//--------------------------------------------------------------------
	_logIndex = 0;
	_logLevel = LOG_LEVEL_SYSTEM;
	wcscpy_s(_logPath, L".");
	setlocale(LC_ALL, "");
}
Logger::~Logger()
{
}
void Logger::SetLogLevel(int logLevel)
{
	_logLevel = logLevel;
}
void Logger::SetLogPath(const wchar_t* logPath)
{
	wcscpy_s(_logPath, logPath);

	int len = wcslen(_logPath);
	if (_logPath[len - 1] == L'\\')
		_logPath[len - 1] = L'\0';
}
void Logger::WriteLog(const wchar_t * type, int logLevel, const wchar_t * fmt, ...)
{
	if (_logLevel > logLevel)
		return;

	wchar_t buffer[256];
	bool truncated;

	va_list args;
	va_start(args, fmt);
	HRESULT ret = StringCchVPrintf(buffer, sizeof(buffer) / 2, fmt, args);
	va_end(args);
	truncated = FAILED(ret);

	//--------------------------------------------------------------------
	// Write Proc
	//--------------------------------------------------------------------
	_logLock.Lock();
	WriteProc(type, logLevel, buffer, truncated);
	_logLock.UnLock();
}
void Logger::WriteHex(const wchar_t* type, int logLevel, const wchar_t* log, BYTE* byte, int byteLen)
{
	if (_logLevel > logLevel)
		return;

	wchar_t hex[4];
	wchar_t buffer[256];
	bool truncated;

	HRESULT ret = StringCchPrintf(buffer, sizeof(buffer) / 2, L"%s - ", log);
	for (int i = 0; i < byteLen && SUCCEEDED(ret); i++)
	{
		StringCchPrintf(hex, sizeof(hex) / 2, L"%02X", byte[i]);
		ret = StringCchCat(buffer, sizeof(buffer) / 2, hex);
	}
	truncated = FAILED(ret);

	//--------------------------------------------------------------------
	// Write Proc
	//--------------------------------------------------------------------
	_logLock.Lock();
	WriteProc(type, logLevel, buffer, truncated);
	_logLock.UnLock();
}
void Logger::WriteProc(const wchar_t* type, int logLevel, const wchar_t* buffer, bool truncated)
{
	const wchar_t* pLogLevel;
	switch (logLevel)
	{
	case LOG_LEVEL_DEBUG:
		pLogLevel = LOG_LEVEL_DEBUGW;
		break;
	case LOG_LEVEL_SYSTEM:
		pLogLevel = LOG_LEVEL_SYSTEMW;
		break;
	case LOG_LEVEL_ERROR:
		pLogLevel = LOG_LEVEL_ERRORW;
		break;
	default:
		pLogLevel = LOG_LEVEL_UNKNOWNW;
		break;
	}

	if (!ExistFile(_logPath))
	{
		if (!MakeDirectory(_logPath))
			SetLogPath(L".");
	}

	tm stTime;
	time_t timer = time(NULL);
	localtime_s(&stTime, &timer);

	wchar_t logFile[MAX_PATH];
	StringCchPrintf(logFile
		, MAX_PATH
		, L"%s\\%d%02d_%s.txt"
		, _logPath
		, stTime.tm_year + 1900
		, stTime.tm_mon + 1
		, type);

	//--------------------------------------------------------------------
	// Write File
	//--------------------------------------------------------------------
	FILE* pFile;
	if (_wfopen_s(&pFile, logFile, L"at") == 0)
	{
		fwprintf_s(pFile
			, L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09d] %s\n"
			, type
			, stTime.tm_year + 1900
			, stTime.tm_mon + 1
			, stTime.tm_mday
			, stTime.tm_hour
			, stTime.tm_min
			, stTime.tm_sec
			, pLogLevel
			, ++_logIndex
			, buffer);

		if (truncated)
		{
			fwprintf_s(pFile
				, L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09d] %s\n"
				, type
				, stTime.tm_year + 1900
				, stTime.tm_mon + 1
				, stTime.tm_mday
				, stTime.tm_hour
				, stTime.tm_min
				, stTime.tm_sec
				, LOG_LEVEL_ERRORW
				, _logIndex
				, LOG_ERROR_BUFFER_TRUNCATED);
		}

		fclose(pFile);
	}
}

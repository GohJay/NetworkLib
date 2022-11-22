#include "Logger.h"
#include <io.h>
#include <stdio.h>
#include <time.h>
#include "locale.h"

using namespace Jay;
int Logger::_logLevel;
Logger Logger::_instance;

Logger::Logger()
{
	_logLevel = LOG_LEVEL_SYSTEM;
	setlocale(LC_ALL, "");
}
Logger::~Logger()
{
}
void Jay::Logger::SetLogLevel(int logLevel)
{
	_logLevel = logLevel;
}
void Logger::WriteLog(const wchar_t * tag, int logLevel, const wchar_t * fmt, ...)
{
	if (_logLevel > logLevel)
		return;

	wchar_t log[512];
	log[0] = L'\0';
	va_list args;
	va_start(args, fmt);
	_vsnwprintf_s(log, sizeof(log), fmt, args);
	va_end(args);

	tm stTime;
	time_t timer = time(NULL);
	localtime_s(&stTime, &timer);
#ifdef _DEBUG
	wprintf_s(L"[%d/%02d/%02d %02d:%02d:%02d] %s\n", stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec, log);
#else
	wchar_t logFile[MAX_PATH];
	swprintf_s(logFile, L"%s_%d-%02d-%02d.log", tag, stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday);
	FILE* pFile;
	if (_wfopen_s(&pFile, logFile, L"at") != 0)
		return;
	fwprintf_s(pFile, L"[%d/%02d/%02d %02d:%02d:%02d] %s\n"
		, stTime.tm_year + 1900, stTime.tm_mon + 1, stTime.tm_mday, stTime.tm_hour, stTime.tm_min, stTime.tm_sec, log);
	fclose(pFile);
#endif // !_DEBUG
}

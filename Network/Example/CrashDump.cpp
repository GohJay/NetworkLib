#include "CrashDump.h"
#include <stdio.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#pragma comment(lib, "DbgHelp.lib")

using namespace Jay;
long CrashDump::_DumpFlag = FALSE;
CrashDump CrashDump::_instance;

CrashDump::CrashDump()
{
	_invalid_parameter_handler oldHandler, newHandler;
	newHandler = myInvalidParameterHandler;
	oldHandler = _set_invalid_parameter_handler(newHandler);	// crt 함수에 null 포인터 등을 넣었을 때

	_CrtSetReportMode(_CRT_WARN, 0);		// CRT 오류 메시지 표시 중단. 바로 덤프 남도록
	_CrtSetReportMode(_CRT_ASSERT, 0);		// CRT 오류 메시지 표시 중단. 바로 덤프 남도록
	_CrtSetReportMode(_CRT_ERROR, 0);		// CRT 오류 메시지 표시 중단. 바로 덤프 남도록
	_CrtSetReportHook(_custom_Report_hook);

	_set_purecall_handler(myPureCallHandler);
	SetHandlerDump();
}
CrashDump::~CrashDump()
{
}
void CrashDump::Crash(void)
{
	int* p = nullptr;
	*p = 0;
}
LONG __stdcall CrashDump::MyExceptionFilter(PEXCEPTION_POINTERS pExceptionPointer)
{
	//--------------------------------------------------------------------
	// 다른 스레드에서 이미 덤프 파일을 생성중인 경우 Sleep
	//--------------------------------------------------------------------
	if (InterlockedExchange(&_DumpFlag, TRUE) == TRUE)
		Sleep(INFINITE);

	//--------------------------------------------------------------------
	// 현재 날짜와 시간을 알아온다.
	//--------------------------------------------------------------------
	SYSTEMTIME stNowTime;
	WCHAR filename[MAX_PATH];
	GetLocalTime(&stNowTime);

	swprintf(filename, MAX_PATH, L"Dump_%d%02d%02d_%02d.%02d.%02d.dmp",
		stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
	wprintf_s(L"\n\n\n!!! Crash Error !!! %d.%02d.%02d / %02d:%02d:%02d\n",
		stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
	wprintf_s(L"Now save dump file...\n");

	//--------------------------------------------------------------------
	// 덤프 파일을 생성한다.
	//--------------------------------------------------------------------
	HANDLE hDumpFile = CreateFile(
		filename,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	//--------------------------------------------------------------------
	// 덤프 파일에 현재 프로세스의 모든 메모리 상태를 로깅한다.
	//--------------------------------------------------------------------
	if (hDumpFile != INVALID_HANDLE_VALUE)
	{
		_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;
		MinidumpExceptionInformation.ThreadId = GetCurrentThreadId();
		MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
		MinidumpExceptionInformation.ClientPointers = TRUE;

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			hDumpFile,
			MiniDumpWithFullMemory,
			&MinidumpExceptionInformation,
			NULL,
			NULL);

		CloseHandle(hDumpFile);
		wprintf_s(L"Crash dump file save finish!\n");
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
void CrashDump::SetHandlerDump(void)
{
	SetUnhandledExceptionFilter(MyExceptionFilter);
}
void CrashDump::myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved)
{
	Crash();
}
int CrashDump::_custom_Report_hook(int ireposttype, char* message, int* returnvalue)
{
	Crash();
	return true;
}
void CrashDump::myPureCallHandler(void)
{
	Crash();
}

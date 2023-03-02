#ifndef __FILE_UTIL__H_
#define __FILE_UTIL__H_
#include <Windows.h>

namespace Jay
{
	bool ExistFile(const wchar_t* filepath);
	bool MakeDirectory(const wchar_t* filepath);
	bool GetParentDirectory(const wchar_t* filepath, wchar_t* parentpath);
	size_t GetFileSize(const wchar_t* filepath);
	size_t GetFolderSize(const wchar_t* filepath);
	bool Rename(const wchar_t* oldfile, const wchar_t* newfile, bool force = false);
	bool GetModuleName(HANDLE hProcess, wchar_t* modulename, int size);
}

#endif !__FILE_UTIL__H_

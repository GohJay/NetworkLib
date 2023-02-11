#include "FileUtil.h"
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <psapi.h>

bool Jay::ExistFile(const wchar_t* filepath)
{
	return _waccess_s(filepath, 0) == 0;
}
bool Jay::MakeDirectory(const wchar_t* filepath)
{
	if (wcscmp(filepath, L"") == 0 || wcscmp(filepath, L".") == 0)
		return true;

	if (ExistFile(filepath))
		return true;

	wchar_t parentpath[MAX_PATH];
	if (GetParentDirectory(filepath, parentpath))
		MakeDirectory(parentpath);

	if (CreateDirectory(filepath, NULL))
		return true;

	return false;
}
bool Jay::GetParentDirectory(const wchar_t* filepath, wchar_t* parentpath)
{
	int len;
	wchar_t* offset;

	len = wcslen(filepath);
	wcscpy_s(parentpath, len * 2, filepath);
	offset = &parentpath[len - 1];

	while (offset > parentpath)
	{
		if (*offset == L'\\')
		{
			*offset = L'\0';
			return true;
		}
		offset--;
	}
	return false;
}
size_t Jay::GetFileSize(const wchar_t* filepath)
{
	size_t size = 0;
	FILE* pFile;

	if (_wfopen_s(&pFile, filepath, L"rb") == 0)
	{
		fseek(pFile, 0, SEEK_END);
		size = ftell(pFile);
		fclose(pFile);
	}
	return size;
}
size_t Jay::GetFolderSize(const wchar_t* filepath)
{
	size_t size = 0;
	wchar_t path[MAX_PATH];
	swprintf_s(path, L"%s\\*", filepath);

	HANDLE hFindFile;
	WIN32_FIND_DATA data;
	hFindFile = FindFirstFile(path, &data);
	if (hFindFile == INVALID_HANDLE_VALUE)
		return 0;

	do
	{
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
			continue;

		if (data.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)
		{
			swprintf_s(path, L"%s\\%s", filepath, data.cFileName);
			size += Jay::GetFolderSize(path);
		}
		else
		{
			size += (size_t)((data.nFileSizeHigh * MAXDWORD) + data.nFileSizeLow);
		}
	} while (FindNextFile(hFindFile, &data));

	FindClose(hFindFile);
	return size;
}
bool Jay::Rename(const wchar_t* oldfile, const wchar_t* newfile, bool force)
{
	if (force && ExistFile(newfile))
		_wremove(newfile);
	return _wrename(oldfile, newfile) == 0;
}
bool Jay::GetModuleName(HANDLE hProcess, wchar_t* modulename)
{
	wchar_t modulepath[MAX_PATH];
	wchar_t* filename;

	if (GetModuleFileNameEx(hProcess, NULL, modulepath, MAX_PATH) == 0)
		return false;

	filename = Jay::FindFileName(modulepath);
	Jay::RemoveFileEtc(filename);

	wcscpy_s(modulename, wcslen(filename) * 2, filename);
	return true;
}
wchar_t* Jay::FindFileName(wchar_t* filepath)
{
	for (int i = wcslen(filepath); i > 0; i--)
	{
		if (filepath[i] == '\\')
			return &filepath[i] + 1;
	}
	return filepath;
}
void Jay::RemoveFileEtc(wchar_t* filepath)
{
	for (int i = wcslen(filepath); i > 0; i--)
	{
		if (filepath[i] == '.')
		{
			filepath[i] = '\0';
			break;
		}
	}
}

#include "Lock.h"
#include <stdio.h>
#pragma comment(lib, "Synchronization.lib")

using namespace Jay;

SRWLock::SRWLock()
{
	InitializeSRWLock(&_lock);
}
SRWLock::~SRWLock()
{
}
bool SRWLock::TryLock()
{
	return TryAcquireSRWLockExclusive(&_lock);
}
void SRWLock::Lock()
{
	AcquireSRWLockExclusive(&_lock);
}
void SRWLock::UnLock()
{
	ReleaseSRWLockExclusive(&_lock);
}
bool SRWLock::TryLock_Shared()
{
	return TryAcquireSRWLockShared(&_lock);
}
void SRWLock::Lock_Shared()
{
	AcquireSRWLockShared(&_lock);
}
void SRWLock::UnLock_Shared()
{
	ReleaseSRWLockShared(&_lock);
}

CSLock::CSLock()
{
	InitializeCriticalSection(&_lock);
}
CSLock::~CSLock()
{
	DeleteCriticalSection(&_lock);
}
bool CSLock::TryLock()
{
	return TryEnterCriticalSection(&_lock);
}
void CSLock::Lock()
{
	EnterCriticalSection(&_lock);
}
void CSLock::UnLock()
{
	LeaveCriticalSection(&_lock);
}

AddressLock::AddressLock() : _lock(FALSE)
{
}
AddressLock::~AddressLock()
{
}
bool AddressLock::TryLock()
{
	return (InterlockedExchange(&_lock, TRUE) == FALSE);
}
void AddressLock::Lock()
{
	long compare = TRUE;
	while (InterlockedExchange(&_lock, TRUE) != FALSE)
	{
		WaitOnAddress(&_lock, &compare, sizeof(long), INFINITE);
	}
}
void AddressLock::UnLock()
{
	_lock = FALSE;
	WakeByAddressSingle((void*)&_lock);
}

SpinLock::SpinLock() : _lock(FALSE)
{
}
SpinLock::~SpinLock()
{
}
bool SpinLock::TryLock()
{
	return (InterlockedExchange(&_lock, TRUE) == FALSE);
}
void SpinLock::Lock()
{
	while (InterlockedExchange(&_lock, TRUE) != FALSE)
	{
		YieldProcessor();
	}
}
void SpinLock::UnLock()
{
	_lock = FALSE;
}

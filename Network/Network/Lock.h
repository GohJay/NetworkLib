#ifndef  __LOCK__H_
#define  __LOCK__H_
#include <Windows.h>

namespace Jay
{
	class SRWLock
	{
		/**
		* @file		Lock.h
		* @brief	SRWLOCK Wrapping Class
		* @details  Slim Reader-Writer Lock Class
		* @author	고재현
		* @date		2023-01-27
		* @version	1.0.3
		**/
	public:
		SRWLock();
		~SRWLock();
	public:
		bool TryLock();
		void Lock();
		void UnLock();
		bool TryLock_Shared();
		void Lock_Shared();
		void UnLock_Shared();
	private:
		SRWLOCK _lock;
	};

	class CSLock
	{
		/**
		* @file		Lock.h
		* @brief	CriticalSection Wrapping Class
		* @details	CriticalSection Lock Class
		* @author	고재현
		* @date		2023-01-27
		* @version	1.0.3
		**/
	public:
		CSLock();
		~CSLock();
	public:
		bool TryLock();
		void Lock();
		void UnLock();
	private:
		CRITICAL_SECTION _lock;
	};

	class AddressLock
	{
		/**
		* @file		Lock.h
		* @brief	AddressLock Class
		* @details	WaitOnAddress Lock Class
		* @author   고재현
		* @date		2023-01-27
		* @version	1.0.3
		**/
	public:
		AddressLock();
		~AddressLock();
	public:
		bool TryLock();
		void Lock();
		void UnLock();
	private:
		volatile long _lock;
	};

	class SpinLock
	{
		/**
		* @file		Lock.h
		* @brief	SpinLock Class
		* @details	Busy-wait Lock Class
		* @author   고재현
		* @date		2023-01-27
		* @version	1.0.3
		**/
	public:
		SpinLock();
		~SpinLock();
	public:
		bool TryLock();
		void Lock();
		void UnLock();
	private:
		volatile long _lock;
	};

	template<typename T>
	class LockGuard
	{
		/**
		* @file		Lock.h
		* @brief	Lock 객체에 대한 Guard Class
		* @details	Lock 객체 사용으로 발생할 수 있는 Dead-lock을 방지하기 위한 Guard Class (Exclusive lock)
		* @author   고재현
		* @date		2023-01-27
		* @version	1.0.2
		**/
	public:
		LockGuard(T* lock) : _lock(lock)
		{
			_lock->Lock();
		}
		~LockGuard()
		{
			_lock->UnLock();
		}
	private:
		T* _lock;
	};

	template<typename T>
	class LockGuard_Shared
	{
		/**
		* @file		Lock.h
		* @brief	Lock 객체에 대한 Guard Class
		* @details	Lock 객체 사용으로 발생할 수 있는 Dead-lock을 방지하기 위한 Guard Class (Shared lock)
		* @author   고재현
		* @date		2023-01-27
		* @version	1.0.0
		**/
	public:
		LockGuard_Shared(T* lock) : _lock(lock)
		{
			_lock->Lock_Shared();
		}
		~LockGuard_Shared()
		{
			_lock->UnLock_Shared();
		}
	private:
		T* _lock;
	};
}

#endif

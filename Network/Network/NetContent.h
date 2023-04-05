#ifndef __NETCONTENT_INFO__H_
#define __NETCONTENT_INFO__H_
#include "NetPacket.h"

namespace Jay
{
	class NetContent
	{
		/**
		* @file		NetContent.h
		* @brief	Content Server Interface Class
		* @details	게임 컨텐츠 서버 제작을 목적으로한 인터페이스 클래스
		* @author	고재현
		* @date		2023-03-31
		* @version	1.0.1
		**/
	public:
		virtual ~NetContent() {}
		virtual void OnStart() = 0;
		virtual void OnStop() = 0;
		virtual void OnUpdate() = 0;
		virtual void OnRecv(DWORD64 sessionID, NetPacket* packet) = 0;
		virtual void OnClientJoin(DWORD64 sessionID) = 0;
		virtual void OnClientLeave(DWORD64 sessionID) = 0;
		virtual void OnContentEnter(DWORD64 sessionID, WPARAM wParam, LPARAM lParam) = 0;
		virtual void OnContentExit(DWORD64 sessionID) = 0;
	};
}

#endif

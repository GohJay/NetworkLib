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
		* @details	���� ������ ���� ������ ���������� �������̽� Ŭ����
		* @author   ������
		* @date		2023-02-20
		* @version  1.0.0
		**/
	public:
		virtual ~NetContent() {}
		virtual void OnUpdate() = 0;
		virtual void OnContentJoin(DWORD64 sessionID) = 0;
		virtual void OnContentLeave(DWORD64 sessionID) = 0;
		virtual void OnRecv(DWORD64 sessionID, NetPacket* packet) = 0;
	};
}

#endif

#ifndef __CONTENT__H
#define __CONTENT__H
#include "NetContent.h"
#include "LFQueue.h"
#include "List.h"

enum CONTENT_JOB_TYPE
{
	JOB_TYPE_CLIENT_JOIN = 0,
	JOB_TYPE_CONTENT_ENTER,
};

struct CONTENT_JOB
{
	CONTENT_JOB_TYPE type;
	DWORD64 sessionID;
	WPARAM wParam;
	LPARAM lParam;
};

struct CONTENT
{
	DWORD threadID;
	WORD contentID;
	WORD frameInterval;
	Jay::NetContent* handler;
	Jay::List<DWORD64> sessionIDList;
	Jay::LFQueue<CONTENT_JOB*> jobQ;
};

#endif

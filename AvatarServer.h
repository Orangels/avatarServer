
// AvatarServer.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

#include "resource.h"		// 主符号
#include <winsock2.h>

// CAvatarServerApp:
// 有关此类的实现，请参阅 AvatarServer.cpp
//

class CAvatarServerApp : public CWinApp
{
	WSADATA m_wsaData;

public:
	CAvatarServerApp();

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CAvatarServerApp theApp;

#pragma once

#include <WinSock2.h>
#include "recorder.h"

class ClientThread
{
	HANDLE m_hThread;
	SOCKET m_Socket;
	Recorder* m_pRecorder;
	BOOL m_bKeepRunning;

public:
	~ClientThread();

	static ClientThread* Create(SOCKET s, Recorder* recorder);
	void Stop();

private:
	ClientThread(SOCKET s, Recorder* recorder);
	ClientThread& operator =(const ClientThread& other);

	static DWORD CALLBACK ThreadProc(LPVOID param);
	void ThreadMain();
};


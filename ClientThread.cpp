#include "ClientThread.h"
#include "Misc.h"


ClientThread::ClientThread(SOCKET s, Recorder* recorder)
{
	m_hThread = NULL;
	m_Socket = s;
	m_pRecorder = recorder;
	m_bKeepRunning = FALSE;
}

ClientThread::~ClientThread()
{
	Stop();
}

ClientThread* ClientThread::Create(SOCKET s, Recorder* recorder)
{
	ClientThread* thread = new ClientThread(s, recorder);
	thread->m_bKeepRunning = TRUE;
	thread->m_hThread = CreateThread(NULL, 0, ClientThread::ThreadProc, thread, 0, NULL);
	return thread;
}

void ClientThread::Stop()
{
	m_bKeepRunning = FALSE;
	if (WAIT_TIMEOUT == WaitForSingleObject(m_hThread, 5000))
		TerminateThread(m_hThread, 0);
	m_hThread = NULL;
}

DWORD ClientThread::ThreadProc(LPVOID param)
{
	ClientThread* self = (ClientThread*)param;
	self->ThreadMain();
	return 0;
}

void ClientThread::ThreadMain()
{
	logger::Log(L"ClientThread start!\n");

	std::vector<unsigned char> pcm;
	while (m_bKeepRunning)
	{
		m_pRecorder->Read(&pcm);
		int n = send(m_Socket, (const char*)pcm.data(), pcm.size(), 0);
		if (n == SOCKET_ERROR)
		{
			logger::Log(L"Send data failed. error: %d\n", WSAGetLastError());
			break;
		}
		else
		{
			//logger::Log(L"Send %d bytes...\n", n);
		}
	}

	closesocket(m_Socket);
	logger::Log(L"ClientThread exit!\n");
}
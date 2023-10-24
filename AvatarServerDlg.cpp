
// AvatarServerDlg.cpp: 实现文件
//

#include "framework.h"
#include "AvatarServer.h"
#include "AvatarServerDlg.h"
#include "afxdialogex.h"
#include "Misc.h"
#include "StringUtil.h"
#include <portaudio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAvatarServerDlg 对话框



CAvatarServerDlg::CAvatarServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AVATARSERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_hServerThread = NULL;
	m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

CAvatarServerDlg::~CAvatarServerDlg()
{
	CloseHandle(m_hExitEvent);
}

void CAvatarServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RECORD_DEVICES, m_wndRecordDevices);
}

BEGIN_MESSAGE_MAP(CAvatarServerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START_REC, &CAvatarServerDlg::OnStartRec)
	ON_BN_CLICKED(IDC_STOP_REC, &CAvatarServerDlg::OnStopRec)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CAvatarServerDlg 消息处理程序

BOOL CAvatarServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	int defaultId = Pa_GetDefaultInputDevice();

	int nCount = Pa_GetDeviceCount();
	for (int i = 0; i < nCount; i++)
	{
		const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
		if (info->maxInputChannels)
		{
			std::wstring text;
			if (i == defaultId)
				text = util::Utf8ToUnicode(info->name) + L" (默认)";
			else
				text = util::Utf8ToUnicode(info->name);
			int id = m_wndRecordDevices.AddString(text.c_str());
			m_wndRecordDevices.SetItemData(id, i);

			if (i == defaultId)
				m_wndRecordDevices.SetCurSel(id);
		}
	}

	if (nCount == 0)
		GetDlgItem(IDC_START_REC)->EnableWindow(FALSE);

	GetDlgItem(IDC_STOP_REC)->EnableWindow(FALSE);

	m_hServerThread = CreateThread(NULL, 0, CAvatarServerDlg::ThreadProc, this, 0, NULL);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CAvatarServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CAvatarServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CAvatarServerDlg::OnStartRec()
{
	int deviceId = m_wndRecordDevices.GetItemData(m_wndRecordDevices.GetCurSel());
	if (!m_Recorder.Open(deviceId, 16000, 1, 16))
	{
		AfxMessageBox(L"打开录音设备失败!", MB_OK | MB_ICONERROR);
		return;
	}

	GetDlgItem(IDC_START_REC)->EnableWindow(FALSE);
	GetDlgItem(IDC_STOP_REC)->EnableWindow(TRUE);
}


void CAvatarServerDlg::OnStopRec()
{
	m_Recorder.Close();
	GetDlgItem(IDC_START_REC)->EnableWindow(TRUE);
	GetDlgItem(IDC_STOP_REC)->EnableWindow(FALSE);
}

DWORD CAvatarServerDlg::ThreadProc(LPVOID param)
{
	CAvatarServerDlg* self = (CAvatarServerDlg*)param;
	self->ServerThreadMain();
	return 0;
}

void CAvatarServerDlg::ServerThreadMain()
{
	int ret = 0;

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET)
	{
		return;
	}

	//绑定IP和端口  
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(8888);
	sin.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(s, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
	{
		closesocket(s);
		return;
	}

	HANDLE hEvent = WSACreateEvent();
	ret = WSAEventSelect(s, hEvent, FD_ACCEPT);

	//开始监听  
	if (listen(s, 5) == SOCKET_ERROR)
	{
		closesocket(s);
		return;
	}

	while (TRUE)
	{
		HANDLE handles[2] = { m_hExitEvent, hEvent };
		ret = WSAWaitForMultipleEvents(2, handles, FALSE, WSA_INFINITE, FALSE);
		if (ret == WAIT_OBJECT_0)
			break;

		WSANETWORKEVENTS networkEvents;
		if (WSAEnumNetworkEvents(s, hEvent, &networkEvents) == SOCKET_ERROR)
		{
			continue;
		}
		else if (networkEvents.lNetworkEvents & FD_ACCEPT)
		{
			WSAResetEvent(hEvent);

			if (networkEvents.iErrorCode[FD_ACCEPT_BIT] != 0) //发生网络错误
			{
				continue;
			}
			else //接受连接请求
			{
				SOCKET sAccept;
				if ((sAccept = accept(s, NULL, NULL)) == INVALID_SOCKET)
				{
					continue;
				}

				int flag = 1;
				setsockopt(sAccept, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

				m_pClientThread.reset(ClientThread::Create(sAccept, &m_Recorder));
			}
		}
	}

	closesocket(s);
	WSACloseEvent(hEvent);
	logger::Log(L"ServerThread exit!\n");
}

void CAvatarServerDlg::OnClose()
{
	SetEvent(m_hExitEvent);
	if (WAIT_TIMEOUT == WaitForSingleObject(m_hServerThread, 5000))
		TerminateThread(m_hServerThread, 0);
	m_hServerThread = NULL;

	CDialogEx::OnClose();
}

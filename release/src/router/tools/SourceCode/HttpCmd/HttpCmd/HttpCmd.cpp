// HttpCmd.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "HttpCmd.h"

#include "textfile.h"

#include <afxinet.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;

using namespace std;

#define DBG_WIN_NAME L"ASUSWRT Test Tool Report"

static ULONG g_PassCnt = 0;
static ULONG g_ErrCnt = 0;

void SendTestReport()
{
	HWND HwndReportApp = ::FindWindow(NULL, DBG_WIN_NAME);
	ULONG TestCnt[2];
	TestCnt[0] = g_PassCnt;
	TestCnt[1] = g_ErrCnt;
	if (HwndReportApp != NULL)
	{
		COPYDATASTRUCT cpd;
		cpd.dwData = ('H' << 8) + 'C';
		cpd.cbData = sizeof(TestCnt);
		cpd.lpData = TestCnt;
		::SendMessage(HwndReportApp, WM_COPYDATA, 0, (LPARAM)&cpd);
	}
}

void SendInitMsg()
{
	WCHAR msg[] = L"HttpCmd start";
	HWND HwndReportApp = ::FindWindow(NULL, DBG_WIN_NAME);
	if (HwndReportApp != NULL)
	{
		COPYDATASTRUCT cpd;
		cpd.dwData = ('D' << 8) + 'B';
		cpd.cbData = (wcslen(msg)+1)*2;
		cpd.lpData = msg;
		::SendMessage(HwndReportApp, WM_COPYDATA, 0, (LPARAM)&cpd);
	}
}

BOOL HttpClient(WCHAR* ConnIp, WCHAR* IdxFn, DWORD& RetCode)
{
	BOOL RetVal = FALSE;
	RetCode = 0;

	try
	{
		CInternetSession Session; 

		/*
		Session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 5000);
		Session.SetOption(INTERNET_OPTION_SEND_TIMEOUT, 3000);
		Session.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 7000);
		Session.SetOption(INTERNET_OPTION_DATA_SEND_TIMEOUT, 3000);
		Session.SetOption(INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, 7000);
		Session.SetOption(INTERNET_OPTION_CONNECT_RETRIES, 1);
		*/

		//CHttpConnection *pHttpConnect = Session.GetHttpConnection( L"192.168.177.1" ) ;
		CHttpConnection *pHttpConnect = Session.GetHttpConnection( ConnIp ) ;
		if( pHttpConnect )
		{
			CHttpFile* pFile = pHttpConnect->OpenRequest( CHttpConnection::HTTP_VERB_GET, 
				IdxFn,
				NULL,
				1,
				NULL,
				NULL,
				INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_DONT_CACHE ); // 

			if ( pFile )
			{   
				pFile->AddRequestHeaders(L"Accept: */*");
				pFile->AddRequestHeaders(L"Accept-Encoding: gzip,deflate");
				pFile->AddRequestHeaders(L"Accept-Language: zh-TW,zh;q=0.8,en-US;q=0.6,en;q=0.4");
				pFile->AddRequestHeaders(L"Authorization: Basic YWRtaW46YWRtaW4=");
				pFile->AddRequestHeaders(L"Connection: keep-alive");
				BOOL RetSendReq = pFile->SendRequest();
				if (RetSendReq)
				{
					BOOL RetStsCode = pFile->QueryInfoStatusCode(RetCode);
					if (RetCode == 200)
					{
						CString data;
						while (pFile->ReadString(data))
						{
						}
						RetVal = TRUE;
					}
				}

				pFile->Close();
				delete pFile;
			}


			pHttpConnect->Close() ;
			delete pHttpConnect ;
		}
	}
	catch( CInternetException *e )
	{
		e->Delete();		
		wprintf(L"Error\n");
	} 

	return RetVal;
}

bool g_Ctrl_C = false;


BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    char mesg[128];

    switch(CEvent)
    {
    case CTRL_C_EVENT:
		wprintf(L"ctrl + c pressed\n");
		g_Ctrl_C = true;
        break;
    case CTRL_BREAK_EVENT:
        break;
    case CTRL_CLOSE_EVENT:
        break;
    case CTRL_LOGOFF_EVENT:
        break;
    case CTRL_SHUTDOWN_EVENT:
        break;

    }
    return TRUE;
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
		return nRetCode;
	}

	wprintf(L"HttpCmd v1.01 (4/18/2012)\n\n",argv[0]);

	if (argc != 6)
	{
		wprintf(L"incorrect parameter. for example, %s 192.168.1.1 index.asp 5 9999 test_result.txt\n\n",argv[0]);
		wprintf(L"192.168.1.1 : httpd server\n");
		wprintf(L"index.asp : file to retrieve\n");
		wprintf(L"5 : delay between two http request\n");
		wprintf(L"9999 : 9999 = infinite loop, other number means loop in how many times\n");
		wprintf(L"test_result.txt : result file\n");
		wprintf(L"PS:\n");
		wprintf(L"server username : admin\n");
		wprintf(L"server password : admin\n");
		return 0;
	}

    if (SetConsoleCtrlHandler( (PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE)
    {
        // unable to install handler... 
        // display message to the user
        wprintf(L"Unable to install handler!\n");
        return 0;
    }

	WCHAR* pServerIp = argv[1];
	WCHAR* pFileFnGet = argv[2];
	WCHAR* pDelaySec = argv[3];
	WCHAR* pLoopTimes = argv[4];
	WCHAR* pResult = argv[5];

	CTextFileWrite myfileOut(pResult, CTextFileWrite::ASCII);

	WCHAR Buf[1024];

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	wsprintf(Buf, L" The start time is: %02d %02d %02d:%02d\r\n", lt.wMonth, lt.wDay, lt.wHour, lt.wMinute);
	wprintf(Buf);
	myfileOut << Buf;

	wsprintf(Buf, L"Delay : %s , Time times : %s\r\n", pDelaySec, pLoopTimes);
	wprintf(Buf);
	myfileOut << Buf;

	wprintf(L"connect to : %s\n",pServerIp);
	WCHAR FileGet[256];
	wcscpy(FileGet,L"\\");
	wcscat(FileGet,pFileFnGet);
	wprintf(L"filename : %s\n",FileGet);
	wprintf(L"Username/Password is admin/admin\n");

	DWORD dwSleepMs = _wtoi(pDelaySec)*1000;
	int TestTimes = _wtoi(pLoopTimes);
	bool NoExit = false;
	if (TestTimes == 9999)
	{
		wprintf(L"Test Times : infinite\n");
		NoExit = true;
	}
	else
	{
		wprintf(L"Test Times : %d\n",TestTimes);
	}

	
	SendInitMsg();


	wprintf(L"\n use CTRL + C to exit\n");

	for (int cnt = 0; cnt<TestTimes || NoExit; cnt++)
	{
		if (g_Ctrl_C) break;
		BOOL RetVal;
		DWORD RetCode;
		RetVal = HttpClient(pServerIp, FileGet, RetCode);

		wsprintf(Buf, L"%d Return Code : %d\r\n",cnt+1,RetCode);
		wprintf(Buf);
		myfileOut << Buf;
		if (RetVal == FALSE)
		{
			g_ErrCnt++;
			myfileOut << L"!!!!! ERROR !!!!!\r\n";
		}
		else
		{
			g_PassCnt++;
		}
		SendTestReport();
		Sleep(dwSleepMs);
	}

	GetLocalTime(&lt);
	wprintf(L" The end time is: %02d %02d %02d:%02d\r\n", lt.wMonth, lt.wDay, lt.wHour, lt.wMinute);

	myfileOut.Close();

	return 1;
}

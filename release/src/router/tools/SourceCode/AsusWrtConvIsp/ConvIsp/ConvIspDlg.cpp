// ConvIspDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ConvIsp.h"
#include "ConvIspDlg.h"
#include "textfile.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CConvIspDlg dialog




CConvIspDlg::CConvIspDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CConvIspDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CConvIspDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CConvIspDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, &CConvIspDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// CConvIspDlg message handlers

BOOL CConvIspDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CConvIspDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CConvIspDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CConvIspDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CConvIspDlg::OnBnClickedButton1()
{
	CString IspTxtFileName;
	CString OutTxtFileName;
	CString OutTxtFileName2;
	IspTxtFileName = L"isp.txt";
	OutTxtFileName = L"ISP_List.txt";
	OutTxtFileName2 = L"ISP_List_IPTV.txt";
	CTextFileRead myfileScan(IspTxtFileName);
	CTextFileWrite myfileOutIptv(OutTxtFileName2, CTextFileWrite::UTF_8);
	if (myfileScan.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
	}


	CString line;
	WCHAR* token[32];
	CString tokenStr[32];
	CString outStr;
	WCHAR OneLine[1024];
	int IptvLoc[200];
	int IptvType[200];
	int IptvIdx=0;
	int LineCnt = 0;
	int SkipIptv = 0;
	int IptvTypeCnt = 11;
	int IptvFirstWr = 0;

	while(!myfileScan.Eof())
	{
		int i = 0;
		int idx = 0;
		myfileScan.ReadLine(line);
		CString tmp = line;
		tmp.Trim();
		if (tmp=="") break;
		wcscpy(OneLine,line.GetBuffer());
		token[i] = &OneLine[idx];
		for (;idx<512; idx++)
		{
			WCHAR aaa = OneLine[idx];
			if (OneLine[idx] == '\t')
			{
				OneLine[idx] = 0;
				idx++;
				break;
			}
		}
		tokenStr[i] = token[i];
		i++;
		while( 1 ) 
		{ 
			//MessageBox(token); 
			token[i] = &OneLine[idx];
			for (;idx<512; idx++)
			{
				if (OneLine[idx] == '\t')
				{
					OneLine[idx] = 0;
					idx++;
					break;
				}
			}
			tokenStr[i] = token[i];
			i++;
			if (i == 9) break;
		}
		// coutry, country, city, isp, vpi, vci, protocol, encap
		if (tokenStr[0] == L"" && tokenStr[1] == L"" && tokenStr[2] == L"" && tokenStr[3] == L"")
		{
			if (SkipIptv == 0)
			{
				IptvLoc[IptvIdx]=LineCnt-1;
				// skip iptv in one same isp
				SkipIptv = 1;
				IptvType[IptvIdx]=IptvTypeCnt;
			}

			if (IptvFirstWr == 0)
			{
				IptvFirstWr = 1;
				outStr.Format(L"[\"%d\",\"%s\",\"%s\",\"%s\",\"%s\"]",IptvTypeCnt,tokenStr[5],tokenStr[6],tokenStr[7],tokenStr[8]);
			}
			else
			{
				outStr.Format(L",[\"%d\",\"%s\",\"%s\",\"%s\",\"%s\"]",IptvTypeCnt,tokenStr[5],tokenStr[6],tokenStr[7],tokenStr[8]);
			}

			myfileOutIptv << (LPTSTR)(LPCTSTR)outStr;
			myfileOutIptv.WriteEndl();
		}
		else
		{
			// a regular line , stop skip iptv in one same isp
			if (SkipIptv)
			{
				IptvIdx++;
				IptvTypeCnt++;
				SkipIptv = 0;			
			}
		}
		LineCnt++;
	}

	// if bottom line is not a regular line , we need add one to have correct number
	if (SkipIptv)
	{
		IptvIdx++;
	}


	myfileScan.Close();
	myfileOutIptv.Close();

	CTextFileRead myfile(IspTxtFileName);
	CTextFileWrite myfile2(OutTxtFileName, CTextFileWrite::UTF_8);
	SkipIptv = 0;
	LineCnt=0;
	int firstIspWr = 0;
	int IspIdxCnt = 0;

	while(!myfile.Eof())
	{
		int i = 0;
		int j;
		int idx = 0;
		myfile.ReadLine(line);
		CString tmp = line;
		tmp.Trim();
		if (tmp=="") break;
		wcscpy(OneLine,line.GetBuffer());
		token[i] = &OneLine[idx];
		for (;idx<512; idx++)
		{
			WCHAR aaa = OneLine[idx];
			if (OneLine[idx] == '\t')
			{
				OneLine[idx] = 0;
				idx++;
				break;
			}
		}
		tokenStr[i] = token[i];
		i++;
		if (tokenStr[0] == L"")
		{
			LineCnt++;
			continue;
		}
		while( 1 ) 
		{ 
			//MessageBox(token); 
			token[i] = &OneLine[idx];
			for (;idx<512; idx++)
			{
				if (OneLine[idx] == '\t')
				{
					OneLine[idx] = 0;
					idx++;
					break;
				}
			}
			tokenStr[i] = token[i];
			i++;
			if (i == 9) break;
		}
		// coutry, country, city, isp, vpi, vci, protocol, encap

		// check current line is at iptv location
		int AtIptvLine = 0;
		for (j=0; j<IptvIdx; j++)
		{
			if (LineCnt == IptvLoc[j])
			{
				AtIptvLine = 1;
				break;
			}
		}

		if (AtIptvLine)
		{
			tokenStr[9].Format(L"%d",IptvType[j]);
		}
		else
		{
			tokenStr[9] = L"";
		}

		if (tokenStr[0] == L"" && tokenStr[1] == L"" && tokenStr[2] == L"" && tokenStr[3] == L"")
		{
			//
			//MessageBox(L"IPTV");
		}
		else
		{
			int TokIdx;
			bool ErrorChar = false;
			for (TokIdx = 0; TokIdx<10; TokIdx++)
			{
				if (tokenStr[TokIdx].FindOneOf(L"\"") != -1)
				{
					CString msg;
					msg.Format(L"Error %d %s",TokIdx,tokenStr[TokIdx]);
					MessageBox(msg);
					ErrorChar = true;
					break;
				}
			}

			if (ErrorChar) break;


			if (firstIspWr == 0)
			{
				firstIspWr = 1;
				outStr.Format(L"[\"%d\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",IspIdxCnt,tokenStr[0],tokenStr[1],
					tokenStr[2],tokenStr[3],tokenStr[4],tokenStr[5],tokenStr[6],tokenStr[7],tokenStr[8],tokenStr[9]);
			}
			else
			{
				outStr.Format(L",[\"%d\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",IspIdxCnt,tokenStr[0],tokenStr[1],
					tokenStr[2],tokenStr[3],tokenStr[4],tokenStr[5],tokenStr[6],tokenStr[7],tokenStr[8],tokenStr[9]);
			}
		}

		myfile2 << (LPTSTR)(LPCTSTR)outStr;
		myfile2.WriteEndl();

		IspIdxCnt++;

		LineCnt++;
	}

	myfile2.Close();

	MessageBox(L"Done");

}



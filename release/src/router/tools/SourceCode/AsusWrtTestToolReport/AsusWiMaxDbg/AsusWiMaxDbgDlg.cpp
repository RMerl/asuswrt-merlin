
// AsusWiMaxDbgDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AsusWiMaxDbg.h"
#include "AsusWiMaxDbgDlg.h"

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


// CAsusWiMaxDbgDlg dialog




CAsusWiMaxDbgDlg::CAsusWiMaxDbgDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAsusWiMaxDbgDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAsusWiMaxDbgDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_Msg);
    DDX_Control(pDX, IDC_EDIT_HTTP_CMD_ERR_CNT, m_CtrlHttpCmdErrCnt);
}

BEGIN_MESSAGE_MAP(CAsusWiMaxDbgDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_WM_COPYDATA()	
	//}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BUTTON1, &CAsusWiMaxDbgDlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CAsusWiMaxDbgDlg::OnBnClickedButton2)
END_MESSAGE_MAP()


// CAsusWiMaxDbgDlg message handlers

BOOL CAsusWiMaxDbgDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
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

void CAsusWiMaxDbgDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CAsusWiMaxDbgDlg::OnPaint()
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
HCURSOR CAsusWiMaxDbgDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CAsusWiMaxDbgDlg::OnBnClickedButton1()
{
    // TODO: Add your control notification handler code here
    CStdioFile cfile_object;
    cfile_object.Open(L"AsusDbgMsg.txt", CFile::modeCreate|CFile::modeWrite);

    int NumOfMsg = m_Msg.GetCount();
    int i;
    for (i=0; i<NumOfMsg; i++)
    {
        CString output;
        m_Msg.GetText(i,output);
        cfile_object.WriteString(output);
    }
    MessageBox(L"Saved");
}


BOOL CAsusWiMaxDbgDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct) 
{
    CString msg;
    WCHAR FromName[3]=L"AB";
    FromName[0] = (WCHAR)((pCopyDataStruct->dwData) >> 8) & 0xff;
    FromName[1] = (WCHAR)(pCopyDataStruct->dwData) & 0xff;    
    if (!wcscmp(FromName,L"DB"))
    {
        msg.Format(L"%s",(LPCWSTR)pCopyDataStruct->lpData);
        m_Msg.AddString(msg);
    }
    else if(!wcscmp(FromName,L"HC"))
    {
        PULONG pUlongPtr = (PULONG)pCopyDataStruct->lpData;
        ULONG PassCnt = *pUlongPtr++;
        ULONG ErrCnt = *pUlongPtr;
        msg.Format(L"%d / %d",PassCnt,ErrCnt);        
        m_CtrlHttpCmdErrCnt.SetWindowText(msg);
    }
    return CDialog::OnCopyData(pWnd, pCopyDataStruct);
}
void CAsusWiMaxDbgDlg::OnBnClickedButton2()
{
    // TODO: Add your control notification handler code here
    int NumOfItems= m_Msg.GetCount();
    int i;
    for (i =0; i<=NumOfItems; i++)
    {
        m_Msg.DeleteString(0);
    }    
}

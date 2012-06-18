
// DictViewerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "textfile.h"
#include "scanner.h"
#include "DictViewer.h"
#include "DictViewerDlg.h"


#define MAX_NUM_STR_ITEM_UPDATE 8192


WCHAR m_StrSrcLoadStrBase[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcLoadStrBaseContent[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcLoadStrToCmp[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcLoadStrToCmpContent[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
int m_NumStrBase;
int m_NumStrToCmp;



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


// CDictViewerDlg dialog




CDictViewerDlg::CDictViewerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDictViewerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_DlgInit = FALSE;
}

void CDictViewerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_Lst);
    DDX_Control(pDX, IDC_COMBO1, m_Combo);
    DDX_Control(pDX, IDC_LIST2, m_Msg);
    DDX_Control(pDX, IDC_STATIC_1, m_Cap);
    DDX_Control(pDX, IDC_BUTTON1, m_Untrans);
}

BEGIN_MESSAGE_MAP(CDictViewerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
    ON_WM_SIZE()
    ON_CBN_SELCHANGE(IDC_COMBO1, &CDictViewerDlg::OnCbnSelchangeCombo1)
    ON_BN_CLICKED(IDC_BUTTON1, &CDictViewerDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// CDictViewerDlg message handlers

BOOL CDictViewerDlg::OnInitDialog()
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
    CTextFileRead myfileCfg(L"DictViewer.ini");
	if (myfileCfg.IsOpen())
	{
	    if (!myfileCfg.Eof())
	    {
		    CString StrCfg;
		    myfileCfg.ReadLine(StrCfg);
		    CString StrCfgTrim = StrCfg.Trim();
	        m_DictFolderPrefix = StrCfgTrim;
	    }
	}
    myfileCfg.Close();

    //	
	m_Combo.AddString(L"English");
	
	CFileFind finder;
    CString findStr = m_DictFolderPrefix + L"*.dict";
	BOOL bWorking = finder.FindFile(findStr);
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		CString fn = finder.GetFileName();
		if (fn.MakeUpper() != L"EN.DICT" && fn.MakeUpper() != L"TEMP.DICT")
		{
		    m_Combo.AddString(fn);
        }
	}	
	
	m_Combo.SetCurSel(0);
	
	//
	
	DWORD NewExtStyle = m_Lst.GetExtendedStyle();
	NewExtStyle |= (LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);
	m_Lst.SetExtendedStyle(NewExtStyle);
	
	LVCOLUMN lvColumn;
	
    CRect rt;
    m_Lst.GetClientRect(&rt);

    int w = rt.Width() / 7;
	lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = w;
	lvColumn.pszText = L"Item Name";
	m_Lst.InsertColumn(0, &lvColumn);	
	
	lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = w * 3;
	lvColumn.pszText = L"English";
	m_Lst.InsertColumn(1, &lvColumn);	
	
	lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = w * 3;
	lvColumn.pszText = L"Some Else";
	m_Lst.InsertColumn(2, &lvColumn);			
	
    LoadEnDict();
    for (int i=0; i<m_NumStrBase; i++)
    {
        LVITEM lvItem;
        lvItem.mask = LVIF_TEXT;
	    lvItem.iItem = i;
	    lvItem.iSubItem = 0;
	    lvItem.pszText = m_StrSrcLoadStrBase[i]; 
	    m_Lst.InsertItem(&lvItem);   
	    
        LVITEM lvItem2;
        lvItem2.mask = LVIF_TEXT;
	    lvItem2.iItem = i;
	    lvItem2.iSubItem = 1;
	    lvItem2.pszText = m_StrSrcLoadStrBaseContent[i]; 
	    m_Lst.SetItem(&lvItem2);   
	}
	
	m_DlgInit = TRUE;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CDictViewerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CDictViewerDlg::OnPaint()
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
HCURSOR CDictViewerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CDictViewerDlg::LoadEnDict()
{
    CString myfileFnStr = m_DictFolderPrefix + L"EN.dict";
	CTextFileRead myfileIn(myfileFnStr);
	if (myfileIn.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return;
	}
	
	m_NumStrBase = 0;

	// load all str item name to array

	while(!myfileIn.Eof())
	{
		CString StrTest;
		myfileIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else
		{
			WCHAR StrContentBuf[4096];
			wcscpy_s(StrContentBuf,_countof(StrContentBuf),StrTestTrim);
			scanner_set(StrContentBuf, wcslen(StrContentBuf));
			token tok;
			WCHAR* pRespStr;
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
		            if (m_NumStrBase < MAX_NUM_STR_ITEM_UPDATE)				
		            {
					    pRespStr = scanner_get_str();
        		        wcscpy_s(m_StrSrcLoadStrBase[m_NumStrBase],_countof(m_StrSrcLoadStrBase[0]),pRespStr);
        		        scanner_get_rest();
        		        pRespStr = scanner_get_str();
        		        pRespStr++;
        		        wcscpy_s(m_StrSrcLoadStrBaseContent[m_NumStrBase],_countof(m_StrSrcLoadStrBaseContent[0]),pRespStr);
        		        m_NumStrBase++;
        		    }			
        		    else
        		    {
        		        m_Msg.AddString(L"no empty string buffer");
        		    }
					break;
				}
				else
				{
					m_Msg.AddString(L"not a string");
					m_Msg.AddString(StrTestTrim);
				}
			}
		}
	}

	myfileIn.Close();
}



void CDictViewerDlg::LoadCmpDict(WCHAR* CmpDict)
{
    CString myfileFnStr = m_DictFolderPrefix + CmpDict;
	CTextFileRead myfileIn(myfileFnStr);
	if (myfileIn.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return;
	}
	
	m_NumStrToCmp = 0;

	// load all str item name to array

	while(!myfileIn.Eof())
	{
		CString StrTest;
		myfileIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			// skip
		}
		else
		{
			WCHAR StrContentBuf[4096];
			wcscpy_s(StrContentBuf,_countof(StrContentBuf),StrTestTrim);
			scanner_set(StrContentBuf, wcslen(StrContentBuf));
			token tok;
			WCHAR* pRespStr;
			while (1)
			{
				tok = scanner();
				if (tok == TOKEN_EOF)
				{
					break;
				}
				if (tok == TOKEN_STRING)
				{
		            if (m_NumStrToCmp < MAX_NUM_STR_ITEM_UPDATE)				
		            {
					    pRespStr = scanner_get_str();
        		        wcscpy_s(m_StrSrcLoadStrToCmp[m_NumStrToCmp],_countof(m_StrSrcLoadStrToCmp[0]),pRespStr);
        		        scanner_get_rest();
        		        pRespStr = scanner_get_str();
        		        pRespStr++;
        		        wcscpy_s(m_StrSrcLoadStrToCmpContent[m_NumStrToCmp],_countof(m_StrSrcLoadStrToCmpContent[0]),pRespStr);
        		        m_NumStrToCmp++;
        		    }			
        		    else
        		    {
        		        m_Msg.AddString(L"no empty string buffer");
        		    }
					break;
				}
				else
				{
					m_Msg.AddString(L"not a string");
					m_Msg.AddString(StrTestTrim);
				}
			}
		}
	}

	myfileIn.Close();
}

void CDictViewerDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    
    // TODO: Add your message handler code here
    if (m_DlgInit)
    {
        //CRect rtClient;    
        //GetClientRect(&rtClient);

        int MsgW;
        int MsgH;
        int LstW;
        int LstH;
    
	    CRect rtMsg;
        m_Msg.GetClientRect(&rtMsg);
        MsgW = rtMsg.Width();
        MsgH = rtMsg.Height();

        CRect rt;
        m_Lst.GetClientRect(&rt);
        LstW = rt.Width();
        LstH = rt.Height();
        
        CRect rtCombo;
        m_Combo.GetClientRect(&rtCombo);        

        // get task bar height        
        CWnd *pTaskbar = CWnd::FindWindow(L"Shell_TrayWnd", NULL);
        CRect rc;
        pTaskbar->GetWindowRect(rc);
        int nWidth = rc.Width();
        int nHeight = rc.Height();
        
        
        int NewLstW = cx;
        int NewLstH = cy - nHeight - rtCombo.Height();
        
        m_Cap.EnableWindow(FALSE);
        m_Cap.ShowWindow(SW_HIDE);
        m_Combo.SetWindowPos(&CWnd::wndBottom, 0, 0, rtCombo.Width(), rtCombo.Height(), 0);
        m_Untrans.SetWindowPos(&CWnd::wndBottom, rtCombo.Width()+20, 0, 0, 0, SWP_NOSIZE);
        m_Lst.SetWindowPos(&CWnd::wndBottom, 0, rtCombo.Height(), NewLstW, NewLstH, 0);
        m_Msg.SetWindowPos(&CWnd::wndBottom, 0, rtCombo.Height() + NewLstH, NewLstW, MsgH, 0);
        
        LVCOLUMN lvColumn;
        int w = (cx - 20) / 7;
	    lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	    lvColumn.fmt = LVCFMT_LEFT;
	    lvColumn.cx = w;
	    lvColumn.pszText = L"Item Name";
	    m_Lst.SetColumn(0, &lvColumn);	
    	
	    lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	    lvColumn.fmt = LVCFMT_LEFT;
	    lvColumn.cx = w * 3;
	    lvColumn.pszText = L"English";
	    m_Lst.SetColumn(1, &lvColumn);	
    	
	    lvColumn.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
	    lvColumn.fmt = LVCFMT_LEFT;
	    lvColumn.cx = w * 3;
	    lvColumn.pszText = L"Some Else";
	    m_Lst.SetColumn(2, &lvColumn);			
        
    }
}

void CDictViewerDlg::OnCbnSelchangeCombo1()
{
    // TODO: Add your control notification handler code here
    CString Sel;
    m_Combo.SetCurSel(m_Combo.GetCurSel());
    m_Combo.GetWindowText(Sel);
    LoadCmpDict((WCHAR*)(LPCTSTR)Sel);
    
    int UnTransCnt = 0;
    for (int i=0; i<m_NumStrBase; i++)
    {
        BOOLEAN found = FALSE;    
        for (int j=0; j<m_NumStrToCmp; j++)
        {
            if (!wcscmp(m_StrSrcLoadStrToCmp[j],m_StrSrcLoadStrBase[i]))
            {
                LVITEM lvItem;
                lvItem.mask = LVIF_TEXT;
	            lvItem.iItem = i;
	            lvItem.iSubItem = 2;
	            lvItem.pszText = m_StrSrcLoadStrToCmpContent[j]; 
	            m_Lst.SetItem(&lvItem);   
	            found = TRUE;
	            break;
	        }
	    }
	    if (found == FALSE)
	    {
            LVITEM lvItem;
            lvItem.mask = LVIF_TEXT;
            lvItem.iItem = i;
            lvItem.iSubItem = 2;
            lvItem.pszText = L""; 
            m_Lst.SetItem(&lvItem);   	    
	        UnTransCnt++;
        }
	}
    CString msg;
    msg.Format(L"%d untranslated strings",UnTransCnt);
    m_Msg.ResetContent();
    m_Msg.AddString(msg);
}

void CDictViewerDlg::OnBnClickedButton1()
{
    // TODO: Add your control notification handler code here
    int UnTransCnt = 0;
    CTextFileWrite myfileOut(L"untrans.txt", CTextFileWrite::UTF_8);
    for (int i=0; i<m_Combo.GetCount(); i++)
    {
        CString Sel;
        m_Combo.SetCurSel(i);
        m_Combo.GetWindowText(Sel);    
        if (Sel != L"English")
        {
            LoadCmpDict((WCHAR*)(LPCTSTR)Sel);
            myfileOut.WriteEndl();
            myfileOut << L"***** ";
            myfileOut << Sel;
            myfileOut << L" *****";
            myfileOut.WriteEndl();
            myfileOut.WriteEndl();
            for (int i=0; i<m_NumStrBase; i++)
            {
                BOOLEAN found = FALSE;    
                for (int j=0; j<m_NumStrToCmp; j++)
                {
                    if (!wcscmp(m_StrSrcLoadStrToCmp[j],m_StrSrcLoadStrBase[i]))
                    {
	                    found = TRUE;
	                    break;
	                }
	            }
	            if (found == FALSE)
	            {
	                myfileOut << m_StrSrcLoadStrBase[i];
	                myfileOut << L"=";
	                myfileOut << m_StrSrcLoadStrBaseContent[i];
	                myfileOut.WriteEndl();
	                UnTransCnt++;
                }
	        }
        }
    }
    myfileOut.Close();
    CString msg;
    msg.Format(L"Total %d untranslated strings",UnTransCnt);
    m_Msg.ResetContent();
    m_Msg.AddString(msg);            
    ::ShellExecute(NULL,L"open",L"untrans.txt",NULL,NULL,SW_SHOWNORMAL);
}

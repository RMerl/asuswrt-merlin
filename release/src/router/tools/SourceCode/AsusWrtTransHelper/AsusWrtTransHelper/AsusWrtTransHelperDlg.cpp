
// AsusWrtTransHelperDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AsusWrtTransHelper.h"
#include "AsusWrtTransHelperDlg.h"

#include "scanner.h"
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


// CAsusWrtTransHelperDlg dialog




CAsusWrtTransHelperDlg::CAsusWrtTransHelperDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CAsusWrtTransHelperDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAsusWrtTransHelperDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_Msg);
    DDX_Control(pDX, IDC_COMBO1, m_ModelSel);
}

BEGIN_MESSAGE_MAP(CAsusWrtTransHelperDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BUTTON1, &CAsusWrtTransHelperDlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON2, &CAsusWrtTransHelperDlg::OnBnClickedButton2)
    ON_BN_CLICKED(IDC_BUTTON4, &CAsusWrtTransHelperDlg::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_BUTTON3, &CAsusWrtTransHelperDlg::OnBnClickedButton3)
END_MESSAGE_MAP()


// CAsusWrtTransHelperDlg message handlers

BOOL CAsusWrtTransHelperDlg::OnInitDialog()
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
	CTextFileRead myfileIn(L"ModelDef.txt");
	if (myfileIn.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return TRUE;
	}
	
	// read modem define file and show to combo box
		
	while(!myfileIn.Eof())
	{
		CString StrTest;
		myfileIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim != L"")
		{
		    m_ModelSel.AddString(StrTestTrim);
		}
	}    
	
	myfileIn.Close();
	
    int nCount = m_ModelSel.GetCount();
    if (nCount > 0) m_ModelSel.SetCurSel(0);
	

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CAsusWrtTransHelperDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CAsusWrtTransHelperDlg::OnPaint()
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
HCURSOR CAsusWrtTransHelperDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

#define MAX_NUM_STR_ITEM_UPDATE 4096

WCHAR m_StrSrcUpdate[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcUpdateStr[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];

void CAsusWrtTransHelperDlg::FindAndReplaceStr(LPCTSTR UpdateTxtFileNameStr)
{
	CString IspTxtFileName;
	CString OutTxtFileName;
	CString TxtFileName = UpdateTxtFileNameStr;

	int nPos = TxtFileName.Find('.');
	CString FileName;
	if(nPos != -1)
	{
		FileName = TxtFileName.Left(nPos + 1);
	}
	else
	{
		FileName = TxtFileName;
	}

	OutTxtFileName = L"asuswrt_output\\" + FileName + L"dict";
	CString TxtFileName2 = L"..\\..\\www\\" + FileName + L"dict";
	CString UpdateTxtFileName = L"word_output\\" + FileName + "txt";
	
	CTextFileRead myfileIn(TxtFileName2);
	CTextFileRead myfileUpdateIn(UpdateTxtFileName);
	CTextFileWrite myfileOut(OutTxtFileName, CTextFileWrite::UTF_8);
	if (myfileIn.IsOpen() == FALSE || myfileUpdateIn.IsOpen() == FALSE || myfileOut.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return;
	}
	
	// read until [###CONTROL_LINE###] (DO NOT REMOVE or EDIT)
	
	BOOL CtrlLineFound = FALSE;
	
	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Find(L"[###CONTROL_LINE###]") != -1)
		{
		    CtrlLineFound = TRUE;
		    break;
		}
	}
	
	if (CtrlLineFound)
	{
	    m_Msg.AddString(L"Control Line found!");
	}
	else
	{
	    m_Msg.AddString(L"!!! No Control Line found !!!");
	    myfileUpdateIn.Close();
	    return;
	}
	
	// found first non-empty and skip the line
	
	BOOL CommentFound = FALSE;
	
	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim == L"COMMENT")
		{
	        CommentFound = TRUE;
		    break;
		}
	}
	
	if (CommentFound)
	{
	    m_Msg.AddString(L"COMMENT column found!");
	}
	else
	{
	    m_Msg.AddString(L"!!! No COMMENT column found !!!");
	    myfileUpdateIn.Close();
	    return;
	}
	

	int NumStrUpdate = 0;

	// read update strings

	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();

		if (NumStrUpdate < MAX_NUM_STR_ITEM_UPDATE)
		{
		    wcscpy_s(m_StrSrcUpdate[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim);
		    
		    // EN
		    CString StrTest2;
		    myfileUpdateIn.ReadLine(StrTest2);
		    CString StrTestTrim2 = StrTest2.Trim();
		    if (UpdateTxtFileNameStr == L"EN.txt")
		    {
                wcscpy_s(m_StrSrcUpdateStr[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim2);		    
		    }		    
		    
		    // TRANSLATED lang

		    myfileUpdateIn.ReadLine(StrTest2);
		    StrTestTrim2 = StrTest2.Trim();	
		    if (UpdateTxtFileNameStr != L"EN.txt")
		    {
		        wcscpy_s(m_StrSrcUpdateStr[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim2);
            }

            // comment		    
		    myfileUpdateIn.ReadLine(StrTest2);
		    
		    NumStrUpdate++;
		}
		else
		{
		    m_Msg.AddString(L"no empty string buffer");
		}


	}

	myfileUpdateIn.Close();

	// read src strings

	while(!myfileIn.Eof())
	{
		CString StrTest;
		myfileIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
        if (StrTestTrim == L"")
        {
            // skip
        }
		else if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			myfileOut << StrTest;
			myfileOut.WriteEndl();
			// skip
		}
		else
		{
		    StrTestTrim.Replace(L"[###UNTRANSLATED###]",L"");
			WCHAR StrContentBuf[1024];
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
					pRespStr = scanner_get_str();
					BOOL StrToUpdate = FALSE;
					int UpdateIdx;
					for (UpdateIdx = 0; UpdateIdx < NumStrUpdate; UpdateIdx++)
					{
						if (!wcscmp(m_StrSrcUpdate[UpdateIdx],pRespStr))
						{
							StrToUpdate = TRUE;
							break;
						}
					}
					if (StrToUpdate == FALSE)
					{
						myfileOut << StrTestTrim;
						myfileOut.WriteEndl();
					}
					else
					{
						myfileOut << m_StrSrcUpdate[UpdateIdx];
						myfileOut << L"=";
						myfileOut << m_StrSrcUpdateStr[UpdateIdx];
						myfileOut.WriteEndl();					
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
	myfileOut.Close();

	m_Msg.AddString(L"Update OK");
}


void CAsusWrtTransHelperDlg::FindAndReplaceStrModelDep(LPCTSTR UpdateTxtFileNameStr,CString& SelText)
{
	CString IspTxtFileName;
	CString OutTxtFileName;
	CString TxtFileName = UpdateTxtFileNameStr;

	int nPos = TxtFileName.Find('.');
	CString FileName;
	if(nPos != -1)
	{
		FileName = TxtFileName.Left(nPos + 1);
	}
	else
	{
		FileName = TxtFileName;
	}

	OutTxtFileName = L"asuswrt_output\\" + FileName + L"dict";
	CString TxtFileName2 = L"..\\..\\www\\" + FileName + L"dict";
	CString UpdateTxtFileName = L"word_output\\" + FileName + "txt";
	
	CTextFileRead myfileIn(TxtFileName2);
	CTextFileRead myfileUpdateIn(UpdateTxtFileName);
	CTextFileWrite myfileOut(OutTxtFileName, CTextFileWrite::UTF_8);
	if (myfileIn.IsOpen() == FALSE || myfileUpdateIn.IsOpen() == FALSE || myfileOut.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return;
	}
	
	// read until [###CONTROL_LINE###] (DO NOT REMOVE or EDIT)
	
	BOOL CtrlLineFound = FALSE;
	
	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Find(L"[###CONTROL_LINE###]") != -1)
		{
		    CtrlLineFound = TRUE;
		    break;
		}
	}
	
	if (CtrlLineFound)
	{
	    m_Msg.AddString(L"Control Line found!");
	}
	else
	{
	    m_Msg.AddString(L"No Control Line found!!!");
	    myfileUpdateIn.Close();
	    return;
	}
	
	// found first non-empty and skip the line
	
	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim != L"")
		{
		    // word to txt tool will split one row to 4 lines
		    // 1+3 = 4 , fetch 3 lines again to advance to correct line
		    myfileUpdateIn.ReadLine(StrTest);
		    myfileUpdateIn.ReadLine(StrTest);
		    myfileUpdateIn.ReadLine(StrTest);
		    break;
		}
	}	

	int NumStrUpdate = 0;

	// read update strings

	while(!myfileUpdateIn.Eof())
	{
		CString StrTest;
		myfileUpdateIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();

		if (NumStrUpdate < MAX_NUM_STR_ITEM_UPDATE)
		{
		    wcscpy_s(m_StrSrcUpdate[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim);
		    
		    // EN
		    CString StrTest2;
		    myfileUpdateIn.ReadLine(StrTest2);
		    CString StrTestTrim2 = StrTest2.Trim();
		    if (UpdateTxtFileNameStr == L"EN.txt")
		    {
                wcscpy_s(m_StrSrcUpdateStr[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim2);		    
		    }		    
		    
		    // TRANSLATED lang

		    myfileUpdateIn.ReadLine(StrTest2);
		    StrTestTrim2 = StrTest2.Trim();	
		    if (UpdateTxtFileNameStr != L"EN.txt")
		    {
		        wcscpy_s(m_StrSrcUpdateStr[NumStrUpdate],_countof(m_StrSrcUpdate[0]),StrTestTrim2);
            }

            // comment		    
		    myfileUpdateIn.ReadLine(StrTest2);
		    
		    NumStrUpdate++;
		}
		else
		{
		    m_Msg.AddString(L"no empty string buffer");
		}


	}

	myfileUpdateIn.Close();

	// read src strings

	while(!myfileIn.Eof())
	{
		CString StrTest;
		myfileIn.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
		if (StrTestTrim.Left(1) == L"[" && StrTestTrim.Right(1) == L"]")
		{
			myfileOut << StrTest;
			myfileOut.WriteEndl();
			// skip
		}
		else
		{
			WCHAR StrContentBuf[1024];
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
					pRespStr = scanner_get_str();
					BOOL StrToUpdate = FALSE;
					int UpdateIdx;
					for (UpdateIdx = 0; UpdateIdx < NumStrUpdate; UpdateIdx++)
					{
						if (!wcscmp(m_StrSrcUpdate[UpdateIdx],pRespStr))
						{
							StrToUpdate = TRUE;
							break;
						}
					}
					if (StrToUpdate == FALSE)
					{
						myfileOut << StrTestTrim;
						myfileOut.WriteEndl();
					}
					else
					{
						myfileOut << m_StrSrcUpdate[UpdateIdx];
						myfileOut << L"=";
						myfileOut << m_StrSrcUpdateStr[UpdateIdx];
						myfileOut.WriteEndl();					
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
	myfileOut.Close();

	m_Msg.AddString(L"Update OK");
}


void CAsusWrtTransHelperDlg::OnBnClickedButton1()
{
    // TODO: Add your control notification handler code here
    m_Msg.ResetContent();
    m_Msg.AddString(L"delete old files...");
    // delete all old files before merge
    CFileFind finder2;
	BOOL bWorking2 = finder2.FindFile(L"asuswrt_output\\*.dict");
	while (bWorking2)
	{
		bWorking2 = finder2.FindNextFile();
		//cout << (LPCTSTR) finder.GetFileName() << endl;
		CString fn = finder2.GetFilePath();
		DeleteFile(fn);
    }

    /////////////////////////
    
	CFileFind finder;
	BOOL bWorking = finder.FindFile(L"word_output\\*.txt");
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		//cout << (LPCTSTR) finder.GetFileName() << endl;
		CString fn = finder.GetFileName();
		fn = fn.MakeUpper();
		CString TmpMsg;
		TmpMsg = fn;
		TmpMsg += L" found";
		m_Msg.AddString(TmpMsg);
		m_Msg.RedrawWindow();
		int IdxCombo = m_ModelSel.GetCurSel();
		CString SelText;
        m_ModelSel.GetLBText(IdxCombo,SelText);
        if (SelText == L"BASE")
        {
		    FindAndReplaceStr((LPCTSTR) finder.GetFileName());
		}
		else
		{
            FindAndReplaceStrModelDep((LPCTSTR) finder.GetFileName(),SelText);		
		}
	}
}


WCHAR m_StrSrcLoadStrBase[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcLoadStrBaseContent[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
WCHAR m_StrSrcLoadStrToCmp[MAX_NUM_STR_ITEM_UPDATE][MAX_TOKEN_BUF];
int m_NumStrBase;
int m_NumStrToCmp;

BOOL CAsusWrtTransHelperDlg::LoadEnStr(LPCTSTR BaseFileName)
{
	CTextFileRead myfileIn(BaseFileName);
	if (myfileIn.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return FALSE;
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
			WCHAR StrContentBuf[1024];
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
        		        wcscpy_s(m_StrSrcLoadStrBaseContent[m_NumStrBase],_countof(m_StrSrcLoadStrBaseContent[0]),StrTestTrim);
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
	return TRUE;
}


BOOL CAsusWrtTransHelperDlg::LoadCmpStr(LPCTSTR CmpFileName)
{
    CString InFileName = L"..\\..\\www\\";
    InFileName += CmpFileName;
	CTextFileRead myfileIn(InFileName);
	if (myfileIn.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return FALSE;
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
        else if (StrTestTrim.Find(L"[###MODELDEP###]") != -1)		
        {
            // skip
            // untranslated will not handle modeldep strings
        }
		else
		{
		    StrTestTrim.Replace(L"[###UNTRANSLATED###]",L"");
			WCHAR StrContentBuf[1024];
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
	return TRUE;
}


void CAsusWrtTransHelperDlg::AutoUntranslated(LPCTSTR UpdateTxtFileNameStr)
{
	CString OutTxtFileName;
	CString TxtFileName = L"..\\..\\www\\";
	TxtFileName += UpdateTxtFileNameStr;

	OutTxtFileName = L"asuswrt_output_untranslated\\";
	OutTxtFileName += UpdateTxtFileNameStr;
	
	CTextFileRead myfileDup(TxtFileName);
	CTextFileWrite myfileOut(OutTxtFileName, CTextFileWrite::UTF_8);
	if (myfileDup.IsOpen() == FALSE || myfileOut.IsOpen() == FALSE)
	{
		MessageBox(L"File not found!");
		return;
	}
	
	// duplicate the file first
	
	while(!myfileDup.Eof())
	{
		CString StrTest;
		myfileDup.ReadLine(StrTest);
		CString StrTestTrim = StrTest.Trim();
	    myfileOut << StrTestTrim;
	    myfileOut.WriteEndl();
	}
	
	myfileDup.Close();

    // find the string that EN has but the other does not have
    int BaseIdx;
    int CmpIdx;
    int UntranslatedCnt = 0;
    for (BaseIdx = 0; BaseIdx<m_NumStrBase; BaseIdx++)
    {
        BOOL CmpStrFound = FALSE;
        for (CmpIdx = 0; CmpIdx<m_NumStrToCmp; CmpIdx++)
        {
            if (!wcscmp(m_StrSrcLoadStrBase[BaseIdx],m_StrSrcLoadStrToCmp[CmpIdx]))
            {
                CmpStrFound = TRUE;
                break;
            }            
        }
        if (CmpStrFound == FALSE)
        {
            UntranslatedCnt++;
            myfileOut << L"[###UNTRANSLATED###]";
            myfileOut << m_StrSrcLoadStrBaseContent[BaseIdx];
            myfileOut.WriteEndl();
        }
    }
    
    if (UntranslatedCnt == 0)
    {
        m_Msg.AddString(L"No untranslated");
    }
    else
    {
        CString msg;
        msg.Format(L"%d untranslated added",UntranslatedCnt);
        m_Msg.AddString(msg);
    }

	return;
}




void CAsusWrtTransHelperDlg::OnBnClickedButton2()
{
    // TODO: Add your control notification handler code here
    CString msg;
    msg = L"This function is discarded. Because it derangements the whole DICT file.\r\n";
    msg += L"It may have another function for this function";
    MessageBox(msg);
    return;
    m_Msg.ResetContent();
    m_Msg.AddString(L"delete old files...");
    // delete all old files before merge
    CFileFind finder2;
	BOOL bWorking2 = finder2.FindFile(L"asuswrt_output_untranslated\\*.dict");
	while (bWorking2)
	{
		bWorking2 = finder2.FindNextFile();
		//cout << (LPCTSTR) finder.GetFileName() << endl;
		CString fn = finder2.GetFilePath();
		DeleteFile(fn);
    }
    
	CFileFind finder;
	
    BOOL RetValLoadEn = LoadEnStr(L"..\\..\\www\\EN.dict");    
    if (FALSE == RetValLoadEn) return;

	BOOL bWorking = finder.FindFile(L"..\\..\\www\\*.dict");
	while (bWorking)
	{
		bWorking = finder.FindNextFile();
		CString fn = finder.GetFileName();
		fn = fn.MakeUpper();
		CString TmpMsg;
		TmpMsg = fn;
		TmpMsg += L" found";
		m_Msg.AddString(TmpMsg);
		m_Msg.RedrawWindow();
		if (fn != L"EN.dict")
		{
		    BOOL RetValLoadCmp = LoadCmpStr(fn);
		    if (RetValLoadCmp)
		    {
		        AutoUntranslated((LPCTSTR) finder.GetFileName());
		    }
		}
	}
}

void CAsusWrtTransHelperDlg::OnBnClickedButton4()
{
    // TODO: Add your control notification handler code here
    CString msg;
    msg = L"1. Untranslated string use EN.dict as baseline file.\r\n";
    msg += L"2. Untranslated string marking only handles base string. It excludes ModelDep strings.";
    MessageBox(msg);
}

void CAsusWrtTransHelperDlg::OnBnClickedButton3()
{
    // TODO: Add your control notification handler code here
    MessageBox(L"Please use dict viewer");
}

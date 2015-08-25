#include "stdafx.h"
#include "PopUpWnd.h"
#include "resource.h"
#include "PocketPJDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define IDC_BTN1     10
#define IDC_BTN2     11


/////////////////////////////////////////////////////////////////////////////
// CPopUpWnd

CPopUpWnd::CPopUpWnd(CPocketPJDlg* pParent)
{
    Create(pParent);
}

CPopUpWnd::~CPopUpWnd()
{
    DestroyWindow();
}

BOOL CPopUpWnd::Create(CPocketPJDlg* pParent)
{
    BOOL bSuccess;

    m_ParentWnd = pParent;

    // Register window class
    CString csClassName = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW,
                                              0,
                                              CBrush(::GetSysColor(COLOR_BTNFACE)));

    // Create popup window
    bSuccess = CreateEx(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST, // Extended style
                        csClassName,                       // Classname
                        _T("PocketPJ"),                    // Title
                        WS_POPUP|WS_BORDER|WS_CAPTION,     // style
                        0,0,                               // position - updated soon.
                        1,1,				   // Size - updated soon
                        pParent->GetSafeHwnd(),            // handle to parent
                        0,                                 // No menu
                        NULL);    
    if (!bSuccess) 
	return FALSE;

    ShowWindow(SW_HIDE);

    // Now create the controls
    CRect TempRect(0,0,10,10);

    /* |SS_LEFTNOWORDWRAP */
    bSuccess = m_Title1.Create(_T("Title1"), WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
			       TempRect, this, IDC_TITLE1);
    if (!bSuccess)
	return FALSE;

    bSuccess = m_Title2.Create(_T("Title2"), WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
			       TempRect, this, IDC_TITLE2);
    if (!bSuccess)
	return FALSE;

    bSuccess = m_Title3.Create(_T("Title3"), WS_CHILD|WS_VISIBLE|SS_NOPREFIX,
			       TempRect, this, IDC_TITLE3);
    if (!bSuccess)
	return FALSE;

    bSuccess = m_Btn1.Create(_T("Button1"), 
                             WS_CHILD|WS_VISIBLE|WS_TABSTOP| BS_PUSHBUTTON, 
                             TempRect, this, IDC_BTN1);
    if (!bSuccess)
	return FALSE;

    bSuccess = m_Btn2.Create(_T("Button2"), 
                             WS_CHILD|WS_VISIBLE|WS_TABSTOP| BS_PUSHBUTTON, 
                             TempRect, this, IDC_BTN2);
    if (!bSuccess)
	return FALSE;

    CFont *ft1 = new CFont, 
	  *ft2 = new CFont, 
	  *ft3 = new CFont;


    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfHeight = 12;
    lstrcpy(lf.lfFaceName, _T("Arial"));
    VERIFY(ft1->CreateFontIndirect(&lf));
    VERIFY(ft3->CreateFontIndirect(&lf));

    lf.lfHeight = 20;
    VERIFY(ft2->CreateFontIndirect(&lf));

    m_Title1.SetFont(ft1, TRUE);
    m_Title2.SetFont(ft2, TRUE);
    m_Title3.SetFont(ft3, TRUE);


    SetWindowSize();

    // Center and show window
    CenterWindow();

    Show();

    return TRUE;
}

void CPopUpWnd::SetContent(const CPopUpContent &content)
{
    m_Title1.SetWindowText(content.m_Title1);
    m_Title2.SetWindowText(content.m_Title2);
    m_Title3.SetWindowText(content.m_Title3);

    if (content.m_Btn1 != "") {
	m_Btn1.SetWindowText(content.m_Btn1);
	m_Btn1.ShowWindow(SW_SHOW);
    } else {
	m_Btn1.ShowWindow(SW_HIDE);
    }

    if (content.m_Btn2 != "") {
	m_Btn2.SetWindowText(content.m_Btn2);
	m_Btn2.ShowWindow(SW_SHOW);
    } else {
	m_Btn2.ShowWindow(SW_HIDE);
    }

    UpdateWindow();
    ShowWindow(SW_SHOW);
}

void CPopUpWnd::SetWindowSize(int width, int height)
{
    enum { H1 = 16, H2 = 40, H3 = 16, S = 5, G = 10, BW=60, BH=20, BG=40};

    CRect rootRect(0, 0, 320, 240);
    int Y;

    MoveWindow((rootRect.Width() - width)/2, (rootRect.Height() - height)/2,
	       width, height);

    m_Title1.MoveWindow(10, Y=S, width-20, H1);
    m_Title2.MoveWindow(10, Y+=H1+G, width-20, H2);
    m_Title3.MoveWindow(10, Y+=H2+G, width-20, H3);

    m_Btn1.MoveWindow((width-2*BW-BG)/2, Y+=H3+G, BW, BH);
    m_Btn2.MoveWindow((width-2*BW-BG)/2+BW+BG, Y, BW, BH);
}

void CPopUpWnd::Hide()  
{ 
    if (!::IsWindow(GetSafeHwnd())) 
        return;

    if (IsWindowVisible())
    {
        ShowWindow(SW_HIDE);
        ModifyStyle(WS_VISIBLE, 0);
    }
}

void CPopUpWnd::Show()  
{ 
    if (!::IsWindow(GetSafeHwnd()))
        return;

    ModifyStyle(0, WS_VISIBLE);
    ShowWindow(SW_SHOWNA);
    RedrawWindow(NULL,NULL,RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
}

BEGIN_MESSAGE_MAP(CPopUpWnd, CWnd)
    //{{AFX_MSG_MAP(CPopUpWnd)
    ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BTN1, OnCancel1)
    ON_BN_CLICKED(IDC_BTN2, OnCancel2)
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CPopUpWnd message handlers

BOOL CPopUpWnd::OnEraseBkgnd(CDC* pDC) 
{
    CBrush backBrush;
    backBrush.CreateSolidBrush(RGB(255,255,255));
    CBrush* pOldBrush = pDC->SelectObject(&backBrush);

    CRect rect;
    pDC->GetClipBox(&rect);     // Erase the area needed
    pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), PATCOPY);
    pDC->SelectObject(pOldBrush);

    return TRUE;
}

void CPopUpWnd::OnCancel1() 
{
    m_ParentWnd->OnPopUpButton(1);
}


void CPopUpWnd::OnCancel2() 
{
    m_ParentWnd->OnPopUpButton(2);
}


BOOL CPopUpWnd::DestroyWindow() 
{
    return CWnd::DestroyWindow();
}

void CPopUpWnd::PeekAndPump()
{
    MSG msg;
    while (::PeekMessage(&msg, NULL,0,0,PM_NOREMOVE)) 
    {
        if (!AfxGetApp()->PumpMessage()) 
        {
            ::PostQuitMessage(0);
            return;
        } 
    }
}


#ifndef __POPUP_WND_H__
#define __POPUP_WND_H__


class CPocketPJDlg;

/////////////////////////////////////////////////////////////////////////////
struct CPopUpContent
{
    CString m_Title1;
    CString m_Title2;
    CString m_Title3;
    CString m_Btn1;
    CString m_Btn2;
};


/////////////////////////////////////////////////////////////////////////////
// CPopUpWnd window

class CPopUpWnd : public CWnd
{
public:
    CPopUpWnd(CPocketPJDlg* pParent);
    virtual ~CPopUpWnd();

    void SetContent(const CPopUpContent &content);
    void Hide();
    void Show();

    void SetWindowSize(int nWindowWidth = 200, int nWindowHeight = 180);

    void PeekAndPump();
    
// Implementation
protected:
    CPocketPJDlg * m_ParentWnd;

    CStatic       m_Title1;
    CStatic       m_Title2;
    CStatic       m_Title3;
    CButton       m_Btn1;
    CButton       m_Btn2;

    BOOL Create(CPocketPJDlg* pParent);

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CPopUpWnd)
	public:
	virtual BOOL DestroyWindow();
	//}}AFX_VIRTUAL

// Generated message map functions
protected:
    //{{AFX_MSG(CPopUpWnd)
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	//}}AFX_MSG
    afx_msg void OnCancel1();
    afx_msg void OnCancel2();
    DECLARE_MESSAGE_MAP()
};


#endif
/////////////////////////////////////////////////////////////////////////////


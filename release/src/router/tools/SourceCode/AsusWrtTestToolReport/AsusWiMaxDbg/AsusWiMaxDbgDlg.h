
// AsusWiMaxDbgDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CAsusWiMaxDbgDlg dialog
class CAsusWiMaxDbgDlg : public CDialog
{
// Construction
public:
	CAsusWiMaxDbgDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_ASUSWIMAXDBG_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedButton1();
    CListBox m_Msg;
    BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg void OnBnClickedButton2();
    CEdit m_CtrlHttpCmdErrCnt;
};

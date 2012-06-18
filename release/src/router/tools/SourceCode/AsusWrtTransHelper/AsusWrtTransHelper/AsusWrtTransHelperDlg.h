
// AsusWrtTransHelperDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CAsusWrtTransHelperDlg dialog
class CAsusWrtTransHelperDlg : public CDialog
{
// Construction
public:
	CAsusWrtTransHelperDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_ASUSWRTTRANSHELPER_DIALOG };

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
    void FindAndReplaceStr(LPCTSTR UpdateTxtFileNameStr);
    BOOL LoadEnStr(LPCTSTR BaseFileName);
    BOOL LoadCmpStr(LPCTSTR CmpFileName);
    void AutoUntranslated(LPCTSTR UpdateTxtFileNameStr);
    void FindAndReplaceStrModelDep(LPCTSTR UpdateTxtFileNameStr,CString& SelText);
    CListBox m_Msg;
    afx_msg void OnBnClickedButton2();
    CComboBox m_ModelSel;
    afx_msg void OnBnClickedButton4();
    afx_msg void OnBnClickedButton3();
};

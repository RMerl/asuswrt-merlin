
// DictViewerDlg.h : header file
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"

// CDictViewerDlg dialog
class CDictViewerDlg : public CDialog
{
// Construction
public:
	CDictViewerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_DICTVIEWER_DIALOG };

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
    CListCtrl m_Lst;
    CComboBox m_Combo;
    void CDictViewerDlg::LoadEnDict();
    void CDictViewerDlg::LoadCmpDict(WCHAR* CmpDict);
    CListBox m_Msg;
    afx_msg void OnSize(UINT nType, int cx, int cy);
    CStatic m_Cap;
    BOOLEAN m_DlgInit;
    afx_msg void OnCbnSelchangeCombo1();
    afx_msg void OnBnClickedButton1();
    CButton m_Untrans;
    CString m_DictFolderPrefix;
};

#if !defined(AFX_SETTINGSDLG_H__46F18E6E_F411_4D9E_BEE9_619D80BC81DC__INCLUDED_)
#define AFX_SETTINGSDLG_H__46F18E6E_F411_4D9E_BEE9_619D80BC81DC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SettingsDlg.h : header file
//
#include <Afxtempl.h>

/////////////////////////////////////////////////////////////////////////////
// Settings
struct CPocketPJSettings
{
    CString	m_Domain;
    CString	m_User;
    CString	m_Password;
    bool	m_UseStun;
    CString	m_StunSrv;
    bool	m_UseIce;
    bool	m_UseSrtp;
    bool	m_UsePublish;
    CString	m_DNS;
    bool	m_EchoSuppress;
    DWORD	m_EcTail;
    bool	m_TCP;
    bool	m_VAD;
    bool	m_AutoAnswer;

    CArray<CString,CString> m_Codecs;
    CArray<CString,CString> m_BuddyList;
    
    CPocketPJSettings();

    // Load from registry
    void    LoadRegistry();
    
    // Save to registry
    void    SaveRegistry();
};


/////////////////////////////////////////////////////////////////////////////
// CSettingsDlg dialog

class CSettingsDlg : public CDialog
{
// Construction
public:
	CSettingsDlg(CPocketPJSettings & cfg, CWnd* pParent = NULL);

// Dialog Data
	//{{AFX_DATA(CSettingsDlg)
	enum { IDD = IDD_SETTING };
	CComboBox	m_Codecs;
	CString	m_Domain;
	BOOL	m_ICE;
	CString	m_Passwd;
	BOOL	m_PUBLISH;
	BOOL	m_SRTP;
	BOOL	m_STUN;
	CString	m_StunSrv;
	CString	m_User;
	CString	m_Dns;
	BOOL	m_EchoSuppress;
	CString	m_EcTail;
	BOOL	m_TCP;
	BOOL	m_VAD;
	BOOL	m_AutoAnswer;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSettingsDlg)
	public:
	virtual int DoModal();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CPocketPJSettings & m_Cfg;

	// Generated message map functions
	//{{AFX_MSG(CSettingsDlg)
	afx_msg void OnStun();
	afx_msg void OnEchoSuppress();
	afx_msg void OnSelchangeCodecs();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SETTINGSDLG_H__46F18E6E_F411_4D9E_BEE9_619D80BC81DC__INCLUDED_)

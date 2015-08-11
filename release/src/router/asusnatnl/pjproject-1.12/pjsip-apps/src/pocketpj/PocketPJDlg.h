// PocketPJDlg.h : header file
//

#if !defined(AFX_POCKETPJDLG_H__DF5F90C9_E72B_4557_9126_AFE75A3ADE9D__INCLUDED_)
#define AFX_POCKETPJDLG_H__DF5F90C9_E72B_4557_9126_AFE75A3ADE9D__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SettingsDlg.h"
#include "PopUpWnd.h"
#include <pjsua-lib/pjsua.h>


/////////////////////////////////////////////////////////////////////////////
// CPocketPJDlg dialog

class CPocketPJDlg : public CDialog
{
// Construction
public:
	CPocketPJDlg(CWnd* pParent = NULL);	// standard constructor

	void OnPopUpButton(int btnNo);
	void OnIncomingCall();

// Dialog Data
	//{{AFX_DATA(CPocketPJDlg)
	enum { IDD = IDD_POCKETPJ_DIALOG };
	CEdit	m_Url;
	CListCtrl	m_BuddyList;
	CStatic	m_BtnUrlAction;
	CStatic	m_BtnAcc;
	CStatic	m_AccId;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPocketPJDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CPocketPJDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnBtnAcc();
	afx_msg void OnBtnAction();
	afx_msg void OnSettings();
	afx_msg void OnUriCall();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnUriAddBuddy();
	afx_msg void OnUriDelBuddy();
	afx_msg void OnAccOnline();
	afx_msg void OnAccInvisible();
	afx_msg void OnClickBuddyList(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	enum PopUpType
	{
	    POPUP_REGISTRATION,
	    POPUP_CALL,

	    POPUP_MAX_TYPE
	};
	enum PopUpElement
	{
	    POPUP_EL_TITLE1,
	    POPUP_EL_TITLE2,
	    POPUP_EL_TITLE3,
	    POPUP_EL_BUTTON1,
	    POPUP_EL_BUTTON2,
	};
	CPopUpWnd	*m_PopUp;
	int		m_PopUpCount;
	BOOL		m_PopUpState[POPUP_MAX_TYPE];
	CPopUpContent	m_PopUpContent[POPUP_MAX_TYPE];

	void PopUp_Show(PopUpType type, 
			const CString& title1,
			const CString& title2,
			const CString& title3,
			const CString& btn1,
			const CString& btn2,
			unsigned userData);
	void PopUp_Modify(PopUpType type,
			  PopUpElement el,
			  const CString& text);
	void PopUp_Hide(PopUpType type);

private:
	CPocketPJSettings m_Cfg;

	void Error(const CString &title, pj_status_t rc);
	BOOL Restart();
	void OnOK();
	int  FindBuddyInCfg(const CString &uri);
	int  FindBuddyInPjsua(const CString &uri);
	void RedrawBuddyList();

private:
	pjsua_acc_id	m_PjsuaAccId;

	void OnRegState();
	void OnCallState();

	// pjsua callbacks
	static void on_call_state(pjsua_call_id call_id, pjsip_event *e);
	static void on_call_media_state(pjsua_call_id call_id);
	static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata);
	static void on_reg_state(pjsua_acc_id acc_id);
	static void on_buddy_state(pjsua_buddy_id buddy_id);
	static void on_pager(pjsua_call_id call_id, const pj_str_t *from, 
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *text);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft eMbedded Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_POCKETPJDLG_H__DF5F90C9_E72B_4557_9126_AFE75A3ADE9D__INCLUDED_)

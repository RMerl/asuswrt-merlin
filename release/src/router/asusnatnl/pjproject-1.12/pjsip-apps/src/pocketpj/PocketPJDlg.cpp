// PocketPJDlg.cpp : implementation file
//

#include "stdafx.h"
#include "PocketPJ.h"
#include "PocketPJDlg.h"
#include <iphlpapi.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TIMER_ID    101
static CPocketPJDlg *theDlg;

/////////////////////////////////////////////////////////////////////////////
// CPocketPJDlg dialog

CPocketPJDlg::CPocketPJDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CPocketPJDlg::IDD, pParent), m_PopUp(NULL)
{
	//{{AFX_DATA_INIT(CPocketPJDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	theDlg = this;

	m_PopUp = new CPopUpWnd(this);
	m_PopUp->Hide();

	unsigned i;
	m_PopUpCount = 0;
	for (i=0; i<POPUP_MAX_TYPE; ++i) {
	    m_PopUpState[i] = FALSE;
	}
}

void CPocketPJDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPocketPJDlg)
	DDX_Control(pDX, IDC_URL, m_Url);
	DDX_Control(pDX, IDC_BUDDY_LIST, m_BuddyList);
	DDX_Control(pDX, IDC_BTN_ACTION, m_BtnUrlAction);
	DDX_Control(pDX, IDC_BTN_ACC, m_BtnAcc);
	DDX_Control(pDX, IDC_ACC_ID, m_AccId);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CPocketPJDlg, CDialog)
	//{{AFX_MSG_MAP(CPocketPJDlg)
	ON_BN_CLICKED(IDC_BTN_ACC, OnBtnAcc)
	ON_BN_CLICKED(IDC_BTN_ACTION, OnBtnAction)
	ON_COMMAND(IDC_ACC_SETTINGS, OnSettings)
	ON_COMMAND(IDC_URI_CALL, OnUriCall)
	ON_WM_TIMER()
	ON_COMMAND(IDC_URI_ADD_BUDDY, OnUriAddBuddy)
	ON_COMMAND(IDC_URI_DEL_BUDDY, OnUriDelBuddy)
	ON_COMMAND(IDC_ACC_ONLINE, OnAccOnline)
	ON_COMMAND(IDC_ACC_INVISIBLE, OnAccInvisible)
	ON_NOTIFY(NM_CLICK, IDC_BUDDY_LIST, OnClickBuddyList)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


void CPocketPJDlg::Error(const CString &title, pj_status_t rc)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    wchar_t werrmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(rc, errmsg, sizeof(errmsg));
    pj_ansi_to_unicode(errmsg, strlen(errmsg), werrmsg, PJ_ARRAY_SIZE(werrmsg));

    AfxMessageBox(title + _T(": ") + werrmsg);
}

BOOL CPocketPJDlg::Restart()
{
    unsigned i;
    pj_status_t status;

    char ver[80];
    sprintf(ver, "PocketPJ/%s", pj_get_version());

    ShowWindow(SW_SHOW);
    PopUp_Show(POPUP_REGISTRATION, ver,
	       "Starting up....", "", "", "", 0);

    KillTimer(TIMER_ID);

    // Destroy first.
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Cleaning up..");
    pjsua_destroy();

    m_BtnAcc.SetBitmap(::LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_OFFLINE)) );
    UpdateWindow();


    // Create
    PopUp_Show(POPUP_REGISTRATION, ver,
	       "Starting up....", "Creating stack..", "", "", 0);

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
	Error(_T("Error in creating library"), status);
	PopUp_Hide(POPUP_REGISTRATION);
	return FALSE;
    }

    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;

    pjsua_config_default(&cfg);
    cfg.max_calls = 1;
    cfg.thread_cnt = 0;
    cfg.user_agent = pj_str(ver);

    cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_call_media_state = &on_call_media_state;
    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_reg_state = &on_reg_state;
    cfg.cb.on_buddy_state = &on_buddy_state;
    cfg.cb.on_pager = &on_pager;

    /* Configure nameserver */
    char nameserver[60];
    {
	FIXED_INFO fi;
	PIP_ADDR_STRING pDNS = NULL;
	ULONG len = sizeof(fi);
	CString err;

	PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Retrieving network parameters..");
	if (GetNetworkParams(&fi, &len) != ERROR_SUCCESS) {
	    err = _T("Info: Error querying network parameters. You must configure DNS server.");
	} else if (fi.CurrentDnsServer) {
	    pDNS = fi.CurrentDnsServer;
	} else if (fi.DnsServerList.IpAddress.String[0] != 0) {
	    pDNS = &fi.DnsServerList;
	} else {
	    err = _T("Info: DNS server not configured. You must configure DNS server.");
	} 
	
	if (err.GetLength()) {
	    if (m_Cfg.m_DNS.GetLength()) {
		pj_unicode_to_ansi((LPCTSTR)m_Cfg.m_DNS, m_Cfg.m_DNS.GetLength(),
				   nameserver, sizeof(nameserver));
		cfg.nameserver_count = 1;
		cfg.nameserver[0] = pj_str(nameserver);
	    } else {
		AfxMessageBox(err);
		pjsua_destroy();
		PopUp_Hide(POPUP_REGISTRATION);
		return FALSE;
	    }
	} else {
	    strcpy(nameserver, pDNS->IpAddress.String);
	    cfg.nameserver_count = 1;
	    cfg.nameserver[0] = pj_str(nameserver);
	}
    }

    char tmp_stun[80];
    if (m_Cfg.m_UseStun) {
	pj_unicode_to_ansi((LPCTSTR)m_Cfg.m_StunSrv, m_Cfg.m_StunSrv.GetLength(),
			   tmp_stun, sizeof(tmp_stun));
	cfg.stun_host = pj_str(tmp_stun);
    }

    pjsua_logging_config_default(&log_cfg);
    log_cfg.msg_logging = PJ_TRUE;
    log_cfg.log_filename = pj_str("\\PocketPJ.TXT");

    pjsua_media_config_default(&media_cfg);
    media_cfg.clock_rate = 8000;
    media_cfg.audio_frame_ptime = 40;
    media_cfg.ec_tail_len = 0;
    media_cfg.ilbc_mode = 30;
    media_cfg.max_media_ports = 8;
    // use default quality setting
    //media_cfg.quality = 5;
    media_cfg.thread_cnt = 1;
    media_cfg.enable_ice = m_Cfg.m_UseIce;
    media_cfg.no_vad = !m_Cfg.m_VAD;

    if (m_Cfg.m_EchoSuppress) {
	media_cfg.ec_options = PJMEDIA_ECHO_SIMPLE;
	media_cfg.ec_tail_len = m_Cfg.m_EcTail;
    }

    // Init
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Initializing..");
    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) {
	Error(_T("Error initializing library"), status);
	pjsua_destroy();
	PopUp_Hide(POPUP_REGISTRATION);
	return FALSE;
    }

    // Create one UDP transport
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Adding UDP transport..");
    pjsua_transport_id transport_id;
    pjsua_transport_config udp_cfg;

    pjsua_transport_config_default(&udp_cfg);
    udp_cfg.port = 0;
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP,
					&udp_cfg, &transport_id);
    if (status != PJ_SUCCESS) {
	Error(_T("Error creating UDP transport"), status);
	pjsua_destroy();
	PopUp_Hide(POPUP_REGISTRATION);
	return FALSE;
    }

    // Always instantiate TCP to support auto-switching to TCP when
    // packet is larger than 1300 bytes. If TCP is disabled when
    // no auto-switching will occur
    if (1) {
	// Create one TCP transport
	PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Adding TCP transport..");
	pjsua_transport_id transport_id;
	pjsua_transport_config tcp_cfg;

	pjsua_transport_config_default(&tcp_cfg);
	tcp_cfg.port = 0;
	status = pjsua_transport_create(PJSIP_TRANSPORT_TCP,
					&tcp_cfg, &transport_id);
	if (status != PJ_SUCCESS) {
	    Error(_T("Error creating TCP transport"), status);
	    pjsua_destroy();
	    PopUp_Hide(POPUP_REGISTRATION);
	    return FALSE;
	}
    }

    // Adjust codecs priority
    pj_str_t tmp;
    pjsua_codec_set_priority(pj_cstr(&tmp, "*"), 0);
    for (i=0; i<(unsigned)m_Cfg.m_Codecs.GetSize(); ++i) {
	CString codec = m_Cfg.m_Codecs.GetAt(i);
	char tmp_nam[80];

	pj_unicode_to_ansi((LPCTSTR)codec, codec.GetLength(),
			   tmp_nam, sizeof(tmp_nam));
	pjsua_codec_set_priority(pj_cstr(&tmp, tmp_nam), (pj_uint8_t)(200-i));
    }

    // Start!
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Starting..");
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
	Error(_T("Error starting library"), status);
	pjsua_destroy();
	PopUp_Hide(POPUP_REGISTRATION);
	return FALSE;
    }

    // Add account
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "Adding account..");
    char domain[80], username[80], passwd[80];
    char id[80], reg_uri[80];
    pjsua_acc_config acc_cfg;

    pj_unicode_to_ansi((LPCTSTR)m_Cfg.m_Domain, m_Cfg.m_Domain.GetLength(),
		       domain, sizeof(domain));
    pj_unicode_to_ansi((LPCTSTR)m_Cfg.m_User, m_Cfg.m_User.GetLength(),
		       username, sizeof(username));
    pj_unicode_to_ansi((LPCTSTR)m_Cfg.m_Password, m_Cfg.m_Password.GetLength(),
		       passwd, sizeof(passwd));

    snprintf(id, sizeof(id), "<sip:%s@%s>", username, domain);
    snprintf(reg_uri, sizeof(reg_uri), "sip:%s", domain);

    pjsua_acc_config_default(&acc_cfg);
    acc_cfg.id = pj_str(id);
    acc_cfg.reg_uri = pj_str(reg_uri);
    acc_cfg.cred_count = 1;
    acc_cfg.cred_info[0].scheme = pj_str("Digest");
    acc_cfg.cred_info[0].realm = pj_str("*");
    acc_cfg.cred_info[0].username = pj_str(username);
    acc_cfg.cred_info[0].data_type = 0;
    acc_cfg.cred_info[0].data = pj_str(passwd);

#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
    acc_cfg.use_srtp = (m_Cfg.m_UseSrtp ? PJMEDIA_SRTP_OPTIONAL : PJMEDIA_SRTP_DISABLED);
    acc_cfg.srtp_secure_signaling = 0;
#endif

    acc_cfg.publish_enabled = m_Cfg.m_UsePublish;
    
    char route[80];
    if (m_Cfg.m_TCP) {
	snprintf(route, sizeof(route), "<sip:%s;lr;transport=tcp>", domain);
	acc_cfg.proxy[acc_cfg.proxy_cnt++] = pj_str(route);
    } else {
	snprintf(route, sizeof(route), "<sip:%s;lr>", domain);
	acc_cfg.proxy[acc_cfg.proxy_cnt++] = pj_str(route);
    }

    status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &m_PjsuaAccId);
    if (status != PJ_SUCCESS) {
	Error(_T("Invalid account settings"), status);
	pjsua_destroy();
	PopUp_Hide(POPUP_REGISTRATION);
	return FALSE;
    }

    CString acc_text = m_Cfg.m_User + _T("@") + m_Cfg.m_Domain;
    m_AccId.SetWindowText(acc_text);

    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE1, acc_text);
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE2, "Registering..");
    PopUp_Modify(POPUP_REGISTRATION, POPUP_EL_TITLE3, "");

    SetTimer(TIMER_ID, 100, NULL);
    return TRUE;
}


void CPocketPJDlg::PopUp_Show( PopUpType type, 
			        const CString& title1,
				const CString& title2,
				const CString& title3,
				const CString& btn1,
				const CString& btn2,
				unsigned userData)
{
    PJ_UNUSED_ARG(userData);

    if (!m_PopUpState[type])
	++m_PopUpCount;

    m_PopUpState[type] = TRUE;

    m_PopUpContent[type].m_Title1 = title1;
    m_PopUpContent[type].m_Title2 = title2;
    m_PopUpContent[type].m_Title3 = title3;
    m_PopUpContent[type].m_Btn1 = btn1;
    m_PopUpContent[type].m_Btn2 = btn2;

    m_PopUp->SetContent(m_PopUpContent[type]);
    m_PopUp->Show();
}

void CPocketPJDlg::PopUp_Modify(PopUpType type,
				PopUpElement el,
				const CString& text)
{
    switch (el) {
    case POPUP_EL_TITLE1:
	m_PopUpContent[type].m_Title1 = text;
	break;
    case POPUP_EL_TITLE2:
	m_PopUpContent[type].m_Title2 = text;
	break;
    case POPUP_EL_TITLE3:
	m_PopUpContent[type].m_Title3 = text;
	break;
    case POPUP_EL_BUTTON1:
	m_PopUpContent[type].m_Btn1 = text;
	break;
    case POPUP_EL_BUTTON2:
	m_PopUpContent[type].m_Btn1 = text;
	break;
    }

    m_PopUp->SetContent(m_PopUpContent[type]);
}

void CPocketPJDlg::PopUp_Hide(PopUpType type)
{
    if (m_PopUpState[type])
	--m_PopUpCount;

    m_PopUpState[type] = FALSE;

    if (m_PopUpCount == 0) {
	m_PopUp->Hide();
	UpdateWindow();
    } else {
	for (int i=POPUP_MAX_TYPE-1; i>=0; --i) {
	    if (m_PopUpState[i]) {
		m_PopUp->SetContent(m_PopUpContent[i]);
		break;
	    }
	}
    }
}

void CPocketPJDlg::OnIncomingCall()
{
    pjsua_call_info ci;

    pjsua_call_get_info(0, &ci);

    PopUp_Show(POPUP_CALL, "Incoming call..", ci.remote_info.ptr, "",
	       "Answer", "Hangup", 0);
    pjsua_call_answer(0, 180, NULL, NULL);
    if (m_Cfg.m_AutoAnswer)
	OnPopUpButton(1);
}

void CPocketPJDlg::OnCallState()
{
    pjsua_call_info ci;

    pjsua_call_get_info(0, &ci);
    
    switch (ci.state) {
    case PJSIP_INV_STATE_NULL:	    /**< Before INVITE is sent or received  */
	break;
    case PJSIP_INV_STATE_CALLING:   /**< After INVITE is sent		    */
	PopUp_Show(POPUP_CALL, "Calling..", ci.remote_info.ptr, "",
		   "", "Hangup", 0);
	break;
    case PJSIP_INV_STATE_INCOMING:  /**< After INVITE is received.	    */
	OnIncomingCall();
	break;
    case PJSIP_INV_STATE_EARLY:	    /**< After response with To tag.	    */
    case PJSIP_INV_STATE_CONNECTING:/**< After 2xx is sent/received.	    */
    case PJSIP_INV_STATE_CONFIRMED:  /**< After ACK is sent/received.	    */
	{
	    CString stateText = ci.state_text.ptr;
	    PopUp_Modify(POPUP_CALL, POPUP_EL_TITLE3, stateText);
	}
	break;
    case PJSIP_INV_STATE_DISCONNECTED:/**< Session is terminated.	    */
	PopUp_Modify(POPUP_CALL, POPUP_EL_TITLE3, "Disconnected");
	PopUp_Hide(POPUP_CALL);
	break;
    }    
}

void CPocketPJDlg::on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    PJ_UNUSED_ARG(e);
    PJ_UNUSED_ARG(call_id);

    theDlg->OnCallState();
}

void CPocketPJDlg::on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);
    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	pjsua_conf_connect(call_info.conf_slot, 0);
	pjsua_conf_connect(0, call_info.conf_slot);
    }
}

void CPocketPJDlg::on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
				    pjsip_rx_data *rdata)
{
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(rdata);

    theDlg->OnIncomingCall();
}

void CPocketPJDlg::OnRegState()
{
    pjsua_acc_info ai;
    pjsua_acc_get_info(m_PjsuaAccId, &ai);

    CString acc_text = m_Cfg.m_User + _T("@") + m_Cfg.m_Domain;

    if (ai.expires>0 && ai.status/100==2) {
	/* Registration success */
	HBITMAP old = m_BtnAcc.SetBitmap(::LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_ONLINE)) );
	PJ_UNUSED_ARG(old);
	acc_text += " (OK)";
	m_AccId.SetWindowText(acc_text);
    } else if (ai.status/100 != 2) {
	acc_text += " (err)";
	Error(_T("SIP registration error"), PJSIP_ERRNO_FROM_SIP_STATUS(ai.status));
	m_AccId.SetWindowText(acc_text);
    }
    PopUp_Hide(POPUP_REGISTRATION);
}

void CPocketPJDlg::on_reg_state(pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(acc_id);

    theDlg->OnRegState();
}

void CPocketPJDlg::on_buddy_state(pjsua_buddy_id buddy_id)
{
    PJ_UNUSED_ARG(buddy_id);

    theDlg->RedrawBuddyList();
}

void CPocketPJDlg::on_pager(pjsua_call_id call_id, const pj_str_t *from, 
			    const pj_str_t *to, const pj_str_t *contact,
			    const pj_str_t *mime_type, const pj_str_t *text)
{
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(from);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(mime_type);
    PJ_UNUSED_ARG(text);
}

/////////////////////////////////////////////////////////////////////////////
// CPocketPJDlg message handlers

BOOL CPocketPJDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	CenterWindow(GetDesktopWindow());	// center to the hpc screen
 
	// TODO: Add extra initialization here
	
	m_Cfg.LoadRegistry();
	//ShowWindow(SW_SHOW);
	m_AccId.SetWindowText(m_Cfg.m_User);

	CImageList *il = new CImageList;
	VERIFY(il->Create(16, 16, ILC_COLOR|ILC_MASK, 2, 4));

	CBitmap *bmp = new CBitmap;
	bmp->LoadBitmap(MAKEINTRESOURCE(IDB_BLANK));
	il->Add(bmp, RGB(255,255,255));
	bmp = new CBitmap;
	bmp->LoadBitmap(MAKEINTRESOURCE(IDB_ONLINE));
	il->Add(bmp, RGB(255,255,255));
	
	m_BuddyList.SetImageList(il, LVSIL_SMALL);

	if (m_Cfg.m_Domain.GetLength()==0 || Restart() == FALSE) {
	    for (;;) {
		CSettingsDlg dlg(m_Cfg);
		if (dlg.DoModal() != IDOK) {
		    EndDialog(IDOK);
		    return TRUE;
		}

		m_Cfg.SaveRegistry();

		if (Restart())
		    break;
	    }
	}

	RedrawBuddyList();
	return TRUE;  // return TRUE  unless you set the focus to a control
}



void CPocketPJDlg::OnBtnAcc() 
{
    CMenu menu;
    VERIFY(menu.LoadMenu(IDR_ACC_MENU));
    CMenu* pPopup = menu.GetSubMenu(0);
    ASSERT(pPopup != NULL);

    RECT r;
    m_BtnAcc.GetWindowRect(&r);
    pPopup->TrackPopupMenu(TPM_LEFTALIGN, r.left+4, r.top+4, this);
}

void CPocketPJDlg::OnBtnAction() 
{
    CMenu menu;
    VERIFY(menu.LoadMenu(IDR_URI_MENU));
    CMenu* pPopup = menu.GetSubMenu(0);
    ASSERT(pPopup != NULL);

    RECT r;
    this->m_BtnUrlAction.GetWindowRect(&r);
    pPopup->TrackPopupMenu(TPM_LEFTALIGN, r.left+4, r.top+4, this);
}

void CPocketPJDlg::OnSettings() 
{
    for (;;) {
	CSettingsDlg dlg(m_Cfg);
	if (dlg.DoModal() != IDOK) {
	    return;
	}

	m_Cfg.SaveRegistry();

	if (Restart())
	    break;
    }
}

void CPocketPJDlg::OnOK()
{
    if (AfxMessageBox(_T("Quit PocketPJ?"), MB_YESNO)==IDYES) {
	KillTimer(TIMER_ID);
	PopUp_Show(POPUP_REGISTRATION, "", "Shutting down..", "", "", "", 0);
	pjsua_destroy();
	CDialog::OnOK();
	PopUp_Hide(POPUP_REGISTRATION);
	m_Cfg.SaveRegistry();
	return;
    }
}

void CPocketPJDlg::OnTimer(UINT nIDEvent) 
{
    pjsua_handle_events(10);
    CDialog::OnTimer(nIDEvent);
}

int  CPocketPJDlg::FindBuddyInPjsua(const CString &Uri)
{
    char uri[80];
    pjsua_buddy_id  id[128];
    unsigned i, count = PJ_ARRAY_SIZE(id);

    if (pjsua_enum_buddies(id, &count) != PJ_SUCCESS)
	return PJSUA_INVALID_ID;
    if (count==0)
	return PJSUA_INVALID_ID;

    pj_unicode_to_ansi((LPCTSTR)Uri, Uri.GetLength(), uri, sizeof(uri));

    for (i=0; i<count; ++i) {
	pjsua_buddy_info bi;
	pjsua_buddy_get_info(id[i], &bi);
	if (pj_strcmp2(&bi.uri, uri)==0)
	    return i;
    }

    return PJSUA_INVALID_ID;
}

int  CPocketPJDlg::FindBuddyInCfg(const CString &uri)
{
    int i;
    for (i=0; i<m_Cfg.m_BuddyList.GetSize(); ++i) {
	if (m_Cfg.m_BuddyList.GetAt(0) == uri) {
	    return i;
	}
    }
    return -1;
}

void CPocketPJDlg::RedrawBuddyList()
{
    int i;

    m_BuddyList.DeleteAllItems();

    for (i=0; i<m_Cfg.m_BuddyList.GetSize(); ++i) {
	int isOnline;
	int id;

	id = FindBuddyInPjsua(m_Cfg.m_BuddyList.GetAt(i));
	if (id != PJSUA_INVALID_ID) {
	    pjsua_buddy_info bi;
	    pjsua_buddy_get_info(id, &bi);
	    isOnline = (bi.status == PJSUA_BUDDY_STATUS_ONLINE);
	} else {
	    isOnline = 0;
	}

	LVITEM lvi;
	memset(&lvi, 0, sizeof(lvi));
	lvi.mask = LVIF_TEXT  | LVIF_IMAGE;
	lvi.iItem = i;
	lvi.iImage = isOnline;
	lvi.pszText = (LPTSTR)(LPCTSTR)m_Cfg.m_BuddyList.GetAt(i);

	m_BuddyList.InsertItem(&lvi);
    }
}

void CPocketPJDlg::OnUriCall() 
{
    char tmp[120];
    CString uri;
    pj_status_t status;

    m_Url.GetWindowText(uri);
    pj_unicode_to_ansi((LPCTSTR)uri, uri.GetLength(), tmp, sizeof(tmp));
    if ((status=pjsua_verify_sip_url(tmp)) != PJ_SUCCESS) {
	Error("The URL is not valid SIP URL", status);
	return;
    }

    pj_str_t dest_uri = pj_str(tmp);
    pjsua_call_id call_id;

    status = pjsua_call_make_call(m_PjsuaAccId, &dest_uri, 0, NULL, NULL, &call_id);

    if (status != PJ_SUCCESS)
	Error("Unable to make call", status);
}

void CPocketPJDlg::OnUriAddBuddy() 
{
    int i;
    char tmp[120];
    CString uri;
    pj_status_t status;

    m_Url.GetWindowText(uri);
    pj_unicode_to_ansi((LPCTSTR)uri, uri.GetLength(), tmp, sizeof(tmp));
    if ((status=pjsua_verify_sip_url(tmp)) != PJ_SUCCESS) {
	Error("The URL is not valid SIP URL", status);
	return;
    }

    for (i=0; i<m_Cfg.m_BuddyList.GetSize(); ++i) {
	if (m_Cfg.m_BuddyList.GetAt(0) == uri) {
	    AfxMessageBox(_T("The URI is already in the buddy list"));
	    return;
	}
    }

    m_Cfg.m_BuddyList.Add(uri);
    RedrawBuddyList();
}

void CPocketPJDlg::OnUriDelBuddy() 
{
    CString uri;

    m_Url.GetWindowText(uri);
    int i = FindBuddyInCfg(uri);
    if (i<0) {
	/* Buddy not found */
	return;
    }

    m_Cfg.m_BuddyList.RemoveAt(i);
    RedrawBuddyList();
    AfxMessageBox(_T("Buddy " + uri + " deleted"));
}

void CPocketPJDlg::OnAccOnline() 
{
    pjsua_acc_set_online_status(m_PjsuaAccId, PJ_TRUE);
    m_BtnAcc.SetBitmap(::LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_ONLINE)) );
}

void CPocketPJDlg::OnAccInvisible() 
{
    pjsua_acc_set_online_status(m_PjsuaAccId, PJ_FALSE);
    m_BtnAcc.SetBitmap(::LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_INVISIBLE)) );
}

void CPocketPJDlg::OnPopUpButton(int btnNo)
{
    if (btnNo == 1) {
	pjsua_call_answer(0, 200, NULL, 0);
	PopUp_Modify(POPUP_CALL, POPUP_EL_BUTTON1, "");
    } else if (btnNo == 2) {
	// Hangup button
	PopUp_Modify(POPUP_CALL, POPUP_EL_TITLE2, "Hang up..");
	PopUp_Modify(POPUP_CALL, POPUP_EL_TITLE3, "");
	pjsua_call_hangup(0, PJSIP_SC_DECLINE, 0, 0);
    }
}

void CPocketPJDlg::OnClickBuddyList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    POSITION pos = m_BuddyList.GetFirstSelectedItemPosition();

    PJ_UNUSED_ARG(pNMHDR);

    if (pos != NULL) {
	int iItem = m_BuddyList.GetNextSelectedItem(pos);
	CString uri = m_BuddyList.GetItemText(iItem, 0);
	m_Url.SetWindowText(uri);
    }
    *pResult = 0;
}

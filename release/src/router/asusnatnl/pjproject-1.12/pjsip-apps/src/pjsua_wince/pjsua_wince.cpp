// pjsua_wince.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "pjsua_wince.h"
#include <commctrl.h>
#include <pjsua-lib/pjsua.h>

#define MAX_LOADSTRING 100

// Global Variables:
static HINSTANCE    hInst;
static HWND	    hMainWnd;
static HWND	    hwndCB;
static HWND	    hwndGlobalStatus, hwndURI, hwndCallStatus;
static HWND	    hwndActionButton, hwndExitButton;



//
// Basic config.
//
#define SIP_PORT	5060


//
// Destination URI (to make call, or to subscribe presence)
//
#define SIP_DST_URI	"sip:192.168.0.7:5061"

//
// Account
//
#define HAS_SIP_ACCOUNT	0	// 0 to disable registration
#define SIP_DOMAIN	"server"
#define SIP_REALM	"server"
#define SIP_USER	"user"
#define SIP_PASSWD	"secret"

//
// Outbound proxy for all accounts
//
#define SIP_PROXY	NULL
//#define SIP_PROXY	"sip:192.168.0.2;lr"


//
// Configure nameserver if DNS SRV is to be used with both SIP
// or STUN (for STUN see other settings below)
//
#define NAMESERVER	NULL
//#define NAMESERVER	"62.241.163.201"

//
// STUN server
#if 0
	// Use this to have the STUN server resolved normally
#   define STUN_DOMAIN	NULL
#   define STUN_SERVER	"stun.fwdnet.net"
#elif 0
	// Use this to have the STUN server resolved with DNS SRV
#   define STUN_DOMAIN	"iptel.org"
#   define STUN_SERVER	NULL
#else
	// Use this to disable STUN
#   define STUN_DOMAIN	NULL
#   define STUN_SERVER	NULL
#endif

//
// Use ICE?
//
#define USE_ICE		0


//
// Globals
//
static pj_pool_t   *g_pool;
static pj_str_t	    g_local_uri;
static int	    g_current_acc;
static int	    g_current_call = PJSUA_INVALID_ID;
static int	    g_current_action;

enum
{
    ID_GLOBAL_STATUS	= 21,
    ID_URI,
    ID_CALL_STATUS,
    ID_POLL_TIMER,
};

enum
{
    ID_MENU_NONE	= 64,
    ID_MENU_CALL,
    ID_MENU_ANSWER,
    ID_MENU_DISCONNECT,
    ID_BTN_ACTION,
};


// Forward declarations of functions included in this code module:
static ATOM		MyRegisterClass	(HINSTANCE, LPTSTR);
BOOL			InitInstance	(HINSTANCE, int);
static void		OnCreate	(HWND hWnd);
static LRESULT CALLBACK	WndProc		(HWND, UINT, WPARAM, LPARAM);



/////////////////////////////////////////////////////////////////////////////

static void OnError(const wchar_t *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    PJ_DECL_UNICODE_TEMP_BUF(werrmsg, PJ_ERR_MSG_SIZE);

    pj_strerror(status, errmsg, sizeof(errmsg));
    
    MessageBox(NULL, PJ_STRING_TO_NATIVE(errmsg, werrmsg, PJ_ERR_MSG_SIZE),
	       title, MB_OK);
}


static void SetLocalURI(const char *uri, int len, bool enabled=true)
{
    wchar_t tmp[128];
    if (len==-1) len=pj_ansi_strlen(uri);
    pj_ansi_to_unicode(uri, len, tmp, PJ_ARRAY_SIZE(tmp));
    SetDlgItemText(hMainWnd, ID_GLOBAL_STATUS, tmp);
    EnableWindow(hwndGlobalStatus, enabled?TRUE:FALSE);
}



static void SetURI(const char *uri, int len, bool enabled=true)
{
    wchar_t tmp[128];
    if (len==-1) len=pj_ansi_strlen(uri);
    pj_ansi_to_unicode(uri, len, tmp, PJ_ARRAY_SIZE(tmp));
    SetDlgItemText(hMainWnd, ID_URI, tmp);
    EnableWindow(hwndURI, enabled?TRUE:FALSE);
}


static void SetCallStatus(const char *state, int len)
{
    wchar_t tmp[128];
    if (len==-1) len=pj_ansi_strlen(state);
    pj_ansi_to_unicode(state, len, tmp, PJ_ARRAY_SIZE(tmp));
    SetDlgItemText(hMainWnd, ID_CALL_STATUS, tmp);
}

static void SetAction(int action, bool enable=true)
{
    HMENU hMenu;

    hMenu = CommandBar_GetMenu(hwndCB, 0);

    RemoveMenu(hMenu, ID_MENU_NONE, MF_BYCOMMAND);
    RemoveMenu(hMenu, ID_MENU_CALL, MF_BYCOMMAND);
    RemoveMenu(hMenu, ID_MENU_ANSWER, MF_BYCOMMAND);
    RemoveMenu(hMenu, ID_MENU_DISCONNECT, MF_BYCOMMAND);

    switch (action) {
    case ID_MENU_NONE:
	InsertMenu(hMenu, ID_EXIT, MF_BYCOMMAND, action, TEXT("None"));
	SetWindowText(hwndActionButton, TEXT("-"));
	break;
    case ID_MENU_CALL:
	InsertMenu(hMenu, ID_EXIT, MF_BYCOMMAND, action, TEXT("Call"));
	SetWindowText(hwndActionButton, TEXT("&Call"));
	break;
    case ID_MENU_ANSWER:
	InsertMenu(hMenu, ID_EXIT, MF_BYCOMMAND, action, TEXT("Answer"));
	SetWindowText(hwndActionButton, TEXT("&Answer"));
	break;
    case ID_MENU_DISCONNECT:
	InsertMenu(hMenu, ID_EXIT, MF_BYCOMMAND, action, TEXT("Hangup"));
	SetWindowText(hwndActionButton, TEXT("&Hangup"));
	break;
    }

    EnableMenuItem(hMenu, action, MF_BYCOMMAND | (enable?MF_ENABLED:MF_GRAYED));
    DrawMenuBar(hMainWnd);

    g_current_action = action;
}


/*
 * Handler when invite state has changed.
 */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) {

	g_current_call = PJSUA_INVALID_ID;
	SetURI(SIP_DST_URI, -1);
	SetAction(ID_MENU_CALL);
	//SetCallStatus(call_info.state_text.ptr, call_info.state_text.slen);
	SetCallStatus(call_info.last_status_text.ptr, call_info.last_status_text.slen);

    } else {
	//if (g_current_call == PJSUA_INVALID_ID)
	//    g_current_call = call_id;

	if (call_info.remote_contact.slen)
	    SetURI(call_info.remote_contact.ptr, call_info.remote_contact.slen, false);
	else
	    SetURI(call_info.remote_info.ptr, call_info.remote_info.slen, false);

	if (call_info.state == PJSIP_INV_STATE_CONFIRMED)
	    SetAction(ID_MENU_DISCONNECT);

	SetCallStatus(call_info.state_text.ptr, call_info.state_text.slen);
    }
}


/*
 * Callback on media state changed event.
 * The action may connect the call to sound device, to file, or
 * to loop the call.
 */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	pjsua_conf_connect(call_info.conf_slot, 0);
	pjsua_conf_connect(0, call_info.conf_slot);
    }
}


/**
 * Handler when there is incoming call.
 */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    if (g_current_call != PJSUA_INVALID_ID) {
	pj_str_t reason;
	reason = pj_str("Another call is in progress");
	pjsua_call_answer(call_id, PJSIP_SC_BUSY_HERE, &reason, NULL);
	return;
    }

    g_current_call = call_id;

    pjsua_call_get_info(call_id, &call_info);

    SetAction(ID_MENU_ANSWER);
    SetURI(call_info.remote_info.ptr, call_info.remote_info.slen, false);
    pjsua_call_answer(call_id, 200, NULL, NULL);
}


/*
 * Handler registration status has changed.
 */
static void on_reg_state(pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(acc_id);

    // Log already written.
}


/*
 * Handler on buddy state changed.
 */
static void on_buddy_state(pjsua_buddy_id buddy_id)
{
    /* Currently this is not processed */
    PJ_UNUSED_ARG(buddy_id);
}


/**
 * Incoming IM message (i.e. MESSAGE request)!
 */
static void on_pager(pjsua_call_id call_id, const pj_str_t *from, 
		     const pj_str_t *to, const pj_str_t *contact,
		     const pj_str_t *mime_type, const pj_str_t *text)
{
    /* Currently this is not processed */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(from);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(mime_type);
    PJ_UNUSED_ARG(text);
}


/**
 * Received typing indication
 */
static void on_typing(pjsua_call_id call_id, const pj_str_t *from,
		      const pj_str_t *to, const pj_str_t *contact,
		      pj_bool_t is_typing)
{
    /* Currently this is not processed */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(from);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(is_typing);
}

/** 
 * Callback upon NAT detection completion 
 */
static void nat_detect_cb(const pj_stun_nat_detect_result *res)
{
    if (res->status != PJ_SUCCESS) {
	char msg[250];
	pj_ansi_snprintf(msg, sizeof(msg), "NAT detection failed: %s",
			 res->status_text);
	SetCallStatus(msg, pj_ansi_strlen(msg));
    } else {
	char msg[250];
	pj_ansi_snprintf(msg, sizeof(msg), "NAT type is %s",
			 res->nat_type_name);
	SetCallStatus(msg, pj_ansi_strlen(msg));
    }
}


static BOOL OnInitStack(void)
{
    pjsua_config	    cfg;
    pjsua_logging_config    log_cfg;
    pjsua_media_config	    media_cfg;
    pjsua_transport_config  udp_cfg;
    pjsua_transport_config  rtp_cfg;
    pjsua_transport_id	    transport_id;
    pjsua_transport_info    transport_info;
    pj_str_t		    tmp;
    pj_status_t status;

    /* Create pjsua */
    status = pjsua_create();
    if (status != PJ_SUCCESS) {
	OnError(TEXT("Error creating pjsua"), status);
	return FALSE;
    }

    /* Create global pool for application */
    g_pool = pjsua_pool_create("pjsua", 4000, 4000);

    /* Init configs */
    pjsua_config_default(&cfg);
    pjsua_media_config_default(&media_cfg);
    pjsua_transport_config_default(&udp_cfg);
    udp_cfg.port = SIP_PORT;

    pjsua_transport_config_default(&rtp_cfg);
    rtp_cfg.port = 40000;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.level = 5;
    log_cfg.log_filename = pj_str("\\pjsua.txt");
    log_cfg.msg_logging = 1;
    log_cfg.decor = pj_log_get_decor() | PJ_LOG_HAS_CR;

    /* Setup media */
    media_cfg.clock_rate = 8000;
    media_cfg.ec_options = PJMEDIA_ECHO_SIMPLE;
    media_cfg.ec_tail_len = 256;
    // use default quality setting
    //media_cfg.quality = 1;
    media_cfg.ptime = 20;
    media_cfg.enable_ice = USE_ICE;

    /* Initialize application callbacks */
    cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_call_media_state = &on_call_media_state;
    cfg.cb.on_incoming_call = &on_incoming_call;
    cfg.cb.on_reg_state = &on_reg_state;
    cfg.cb.on_buddy_state = &on_buddy_state;
    cfg.cb.on_pager = &on_pager;
    cfg.cb.on_typing = &on_typing;
    cfg.cb.on_nat_detect = &nat_detect_cb;

    if (SIP_PROXY) {
	    cfg.outbound_proxy_cnt = 1;
	    cfg.outbound_proxy[0] = pj_str(SIP_PROXY);
    }
    
    if (NAMESERVER) {
	    cfg.nameserver_count = 1;
	    cfg.nameserver[0] = pj_str(NAMESERVER);
    }
    
    if (NAMESERVER && STUN_DOMAIN) {
	    cfg.stun_domain = pj_str(STUN_DOMAIN);
    } else if (STUN_SERVER) {
	    cfg.stun_host = pj_str(STUN_SERVER);
    }
    
    
    /* Initialize pjsua */
    status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) {
	OnError(TEXT("Initialization error"), status);
	return FALSE;
    }

    /* Set codec priority */
    pjsua_codec_set_priority(pj_cstr(&tmp, "pcmu"), 240);
    pjsua_codec_set_priority(pj_cstr(&tmp, "pcma"), 230);
    pjsua_codec_set_priority(pj_cstr(&tmp, "speex/8000"), 190);
    pjsua_codec_set_priority(pj_cstr(&tmp, "ilbc"), 189);
    pjsua_codec_set_priority(pj_cstr(&tmp, "speex/16000"), 180);
    pjsua_codec_set_priority(pj_cstr(&tmp, "speex/32000"), 0);
    pjsua_codec_set_priority(pj_cstr(&tmp, "gsm"), 100);


    /* Add UDP transport and the corresponding PJSUA account */
    status = pjsua_transport_create(PJSIP_TRANSPORT_UDP,
				    &udp_cfg, &transport_id);
    if (status != PJ_SUCCESS) {
	OnError(TEXT("Error starting SIP transport"), status);
	return FALSE;
    }

    pjsua_transport_get_info(transport_id, &transport_info);

    g_local_uri.ptr = (char*)pj_pool_alloc(g_pool, 128);
    g_local_uri.slen = pj_ansi_sprintf(g_local_uri.ptr,
				       "<sip:%.*s:%d>",
				       (int)transport_info.local_name.host.slen,
				       transport_info.local_name.host.ptr,
				       transport_info.local_name.port);


    /* Add local account */
    pjsua_acc_add_local(transport_id, PJ_TRUE, &g_current_acc);
    pjsua_acc_set_online_status(g_current_acc, PJ_TRUE);

    /* Add account */
    if (HAS_SIP_ACCOUNT) {
	pjsua_acc_config cfg;

	pjsua_acc_config_default(&cfg);
	cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
	cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
	cfg.cred_count = 1;
	cfg.cred_info[0].realm = pj_str(SIP_REALM);
	cfg.cred_info[0].scheme = pj_str("digest");
	cfg.cred_info[0].username = pj_str(SIP_USER);
	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cfg.cred_info[0].data = pj_str(SIP_PASSWD);

	status = pjsua_acc_add(&cfg, PJ_TRUE, &g_current_acc);
	if (status != PJ_SUCCESS) {
	    pjsua_destroy();
	    return PJ_FALSE;
	}
    }

    /* Add buddy */
    if (SIP_DST_URI) {
    	pjsua_buddy_config bcfg;
    
    	pjsua_buddy_config_default(&bcfg);
    	bcfg.uri = pj_str(SIP_DST_URI);
    	bcfg.subscribe = PJ_FALSE;
    	
    	pjsua_buddy_add(&bcfg, NULL);
    }

    /* Start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
	OnError(TEXT("Error starting pjsua"), status);
	return FALSE;
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance,
		   HINSTANCE hPrevInstance,
		   LPTSTR    lpCmdLine,
		   int       nCmdShow)
{
    MSG msg;
    HACCEL hAccelTable;

    PJ_UNUSED_ARG(lpCmdLine);
    PJ_UNUSED_ARG(hPrevInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow)) 
    {
	return FALSE;
    }
    
    hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_PJSUA_WINCE);
    
    
    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
	if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
	{
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	}
    }
    
    return msg.wParam;
}

static ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
    WNDCLASS wc;

    wc.style		= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc	= (WNDPROC) WndProc;
    wc.cbClsExtra	= 0;
    wc.cbWndExtra	= 0;
    wc.hInstance	= hInstance;
    wc.hIcon		= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PJSUA_WINCE));
    wc.hCursor		= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName	= 0;
    wc.lpszClassName	= szWindowClass;

    return RegisterClass(&wc);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND	hWnd;
    TCHAR	szTitle[MAX_LOADSTRING];
    TCHAR	szWindowClass[MAX_LOADSTRING];

    hInst = hInstance;

    /* Init stack */
    if (OnInitStack() == FALSE)
	return FALSE;

    LoadString(hInstance, IDC_PJSUA_WINCE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance, szWindowClass);

    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
	    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 200, 
	    NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {	
	    return FALSE;
    }

    hMainWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    if (hwndCB)
	CommandBar_Show(hwndCB, TRUE);

    SetTimer(hMainWnd, ID_POLL_TIMER, 50, NULL);

    pjsua_detect_nat_type();
    return TRUE;
}


static void OnCreate(HWND hWnd)
{
    enum 
    {
	X = 10,
	Y = 40,
	W = 220,
	H = 30,
    };

    DWORD dwStyle;

    hMainWnd = hWnd;

    hwndCB = CommandBar_Create(hInst, hWnd, 1);			
    CommandBar_InsertMenubar(hwndCB, hInst, IDM_MENU, 0);   
    CommandBar_AddAdornments(hwndCB, 0, 0);

    // Create global status text
    dwStyle = WS_CHILD | WS_VISIBLE | WS_DISABLED | ES_LEFT;  
    hwndGlobalStatus = CreateWindow(
                TEXT("EDIT"),   // Class name
                NULL,           // Window text
                dwStyle,        // Window style
                X,		// x-coordinate of the upper-left corner
                Y+0,            // y-coordinate of the upper-left corner
                W,		// Width of the window for the edit
                                // control
                H-5,		// Height of the window for the edit
                                // control
                hWnd,           // Window handle to the parent window
                (HMENU) ID_GLOBAL_STATUS, // Control identifier
                hInst,           // Instance handle
                NULL);          // Specify NULL for this parameter when 
                                // you create a control
    SetLocalURI(g_local_uri.ptr, g_local_uri.slen, false);


    // Create URI edit
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER;  
    hwndURI = CreateWindow (
                TEXT("EDIT"),   // Class name
                NULL,		// Window text
                dwStyle,        // Window style
                X,  // x-coordinate of the upper-left corner
                Y+H,  // y-coordinate of the upper-left corner
                W,  // Width of the window for the edit
                                // control
                H-5,  // Height of the window for the edit
                                // control
                hWnd,           // Window handle to the parent window
                (HMENU) ID_URI, // Control identifier
                hInst,           // Instance handle
                NULL);          // Specify NULL for this parameter when 
                                // you create a control

    // Create action Button
    hwndActionButton = CreateWindow( L"button", L"Action", 
                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                         X, Y+2*H, 
			 60, H-5, hWnd, 
                         (HMENU) ID_BTN_ACTION,
                         hInst, NULL );

    // Create exit button
    hwndExitButton = CreateWindow( L"button", L"E&xit", 
                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                         X+70, Y+2*H, 
			 60, H-5, hWnd, 
                         (HMENU) ID_EXIT,
                         hInst, NULL );


    // Create call status edit
    dwStyle = WS_CHILD | WS_VISIBLE | WS_DISABLED;  
    hwndCallStatus = CreateWindow (
                TEXT("EDIT"),   // Class name
                NULL,		// Window text
                dwStyle,        // Window style
                X,  // x-coordinate of the upper-left corner
                Y+3*H,  // y-coordinate of the upper-left corner
                W,  // Width of the window for the edit
                                // control
                H-5,  // Height of the window for the edit
                                // control
                hWnd,           // Window handle to the parent window
                (HMENU) ID_CALL_STATUS, // Control identifier
                hInst,           // Instance handle
                NULL);          // Specify NULL for this parameter when 
                                // you create a control
    SetCallStatus("Ready", 5);
    SetAction(ID_MENU_CALL);
    SetURI(SIP_DST_URI, -1);
    SetFocus(hWnd);

}


static void OnDestroy(void)
{
    pjsua_destroy();
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    
    switch (message) {
    case WM_KEYUP:
	if (wParam==114) {
	    wParam = ID_MENU_CALL;
	} else if (wParam==115) {
	    if (g_current_call == PJSUA_INVALID_ID)
		wParam = ID_EXIT;
	    else
		wParam = ID_MENU_DISCONNECT;
	} else
	    break;

    case WM_COMMAND:
	wmId    = LOWORD(wParam); 
	wmEvent = HIWORD(wParam); 
	if (wmId == ID_BTN_ACTION)
	    wmId = g_current_action;
	switch (wmId)
	{
	case ID_MENU_CALL:
	    if (g_current_call != PJSUA_INVALID_ID) {
		MessageBox(NULL, TEXT("Can not make call"), 
			   TEXT("You already have one call active"), MB_OK);
	    }
	    pj_str_t dst_uri;
	    wchar_t text[256];
	    char tmp[256];
	    pj_status_t status;

	    GetWindowText(hwndURI, text, PJ_ARRAY_SIZE(text));
	    pj_unicode_to_ansi(text, pj_unicode_strlen(text),
			       tmp, sizeof(tmp));
	    dst_uri.ptr = tmp;
	    dst_uri.slen = pj_ansi_strlen(tmp);
	    status = pjsua_call_make_call(g_current_acc,
						      &dst_uri, 0, NULL,
						      NULL, &g_current_call);
	    if (status != PJ_SUCCESS)
		OnError(TEXT("Unable to make call"), status);
	    break;
	case ID_MENU_ANSWER:
	    if (g_current_call == PJSUA_INVALID_ID)
		MessageBox(NULL, TEXT("Can not answer"), 
			   TEXT("There is no call!"), MB_OK);
	    else
		pjsua_call_answer(g_current_call, 200, NULL, NULL);
	    break;
	case ID_MENU_DISCONNECT:
	    if (g_current_call == PJSUA_INVALID_ID)
		MessageBox(NULL, TEXT("Can not disconnect"), 
			   TEXT("There is no call!"), MB_OK);
	    else
		pjsua_call_hangup(g_current_call, PJSIP_SC_DECLINE, NULL, NULL);
	    break;
	case ID_EXIT:
	    DestroyWindow(hWnd);
	    break;
	default:
	    return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;

    case WM_CREATE:
	OnCreate(hWnd);
	break;

    case WM_DESTROY:
	OnDestroy();
	CommandBar_Destroy(hwndCB);
	PostQuitMessage(0);
	break;

    case WM_TIMER:
	pjsua_handle_events(1);
	break;

    default:
	return DefWindowProc(hWnd, message, wParam, lParam);
    }
   return 0;
}


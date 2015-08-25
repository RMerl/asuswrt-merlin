/* $Id: main_wm.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "gui.h"
#include "systest.h"
#include <windows.h>


#include "gui.h"
#include <pjlib.h>
#include <windows.h>
#include <winuserm.h>
#include <aygshell.h>

#define MAINWINDOWCLASS TEXT("SysTestDlg")
#define MAINWINDOWTITLE TEXT("PJSYSTEST")

typedef struct menu_handler_t {
    UINT		 id;
    gui_menu_handler	 handler;
} menu_handler_t;

static HINSTANCE	 g_hInst;
static HWND		 g_hWndMenuBar;
static HWND		 g_hWndMain;
static HWND		 g_hWndLog;
static pj_thread_t	*g_log_thread;
static gui_menu		*g_menu;
static unsigned		 g_menu_handler_cnt;
static menu_handler_t	 g_menu_handlers[64];

static pj_log_func	*g_log_writer_orig;

static pj_status_t gui_update_menu(gui_menu *menu);

static void log_writer(int level, const char *buffer, int len)
{
    wchar_t buf[512];
    int cur_len;

    PJ_UNUSED_ARG(level);

    pj_ansi_to_unicode(buffer, len, buf, 512);

    if (!g_hWndLog)
	return;

    /* For now, ignore log messages from other thread to avoid deadlock */
    if (g_log_thread == pj_thread_this()) {
	cur_len = (int)SendMessage(g_hWndLog, WM_GETTEXTLENGTH, 0, 0);
	SendMessage(g_hWndLog, EM_SETSEL, (WPARAM)cur_len, (LPARAM)cur_len);
	SendMessage(g_hWndLog, EM_REPLACESEL, (WPARAM)0, (LPARAM)buf);
    }
    
    //uncomment to forward to the original log writer
    if (g_log_writer_orig)
	(*g_log_writer_orig)(level, buffer, len);
}

/* execute menu handler for id menu specified, return FALSE if menu handler 
 * is not found.
 */
static BOOL handle_menu(UINT id)
{
    unsigned i;

    for (i = 0; i < g_menu_handler_cnt; ++i) {
	if (g_menu_handlers[i].id == id) {
	    /* menu handler found, execute it */
	    (*g_menu_handlers[i].handler)();
	    return TRUE;
	}
    }

    return FALSE;
}

/* generate submenu and register the menu handler, then return next menu id */
static UINT generate_submenu(HMENU parent, UINT id_start, gui_menu *menu)
{
    unsigned i;
    UINT id = id_start;

    if (!menu)
	return id;

    /* generate submenu */
    for (i = 0; i < menu->submenu_cnt; ++i) {

	if (menu->submenus[i] == NULL) {

	    /* add separator */
	    AppendMenu(parent, MF_SEPARATOR, 0, 0);
	
	}  else if (menu->submenus[i]->submenu_cnt != 0) {
	    
	    /* this submenu item has children, generate popup menu */
	    HMENU hMenu;
	    wchar_t buf[64];
	    
	    pj_ansi_to_unicode(menu->submenus[i]->title, 
			       pj_ansi_strlen(menu->submenus[i]->title),
			       buf, 64);

	    hMenu = CreatePopupMenu();
	    AppendMenu(parent, MF_STRING|MF_ENABLED|MF_POPUP, (UINT)hMenu, buf);
	    id = generate_submenu(hMenu, id, menu->submenus[i]);

	} else {

	    /* this submenu item is leaf, register the handler */
	    wchar_t buf[64];
	    
	    pj_ansi_to_unicode(menu->submenus[i]->title, 
			       pj_ansi_strlen(menu->submenus[i]->title),
			       buf, 64);

	    AppendMenu(parent, MF_STRING, id, buf);

	    if (menu->submenus[i]->handler) {
		g_menu_handlers[g_menu_handler_cnt].id = id;
		g_menu_handlers[g_menu_handler_cnt].handler = 
					menu->submenus[i]->handler;
		++g_menu_handler_cnt;
	    }

	    ++id;
	}
    }

    return id;
}

BOOL InitDialog()
{
    /* update menu */
    if (gui_update_menu(g_menu) != PJ_SUCCESS)
	return FALSE;

    return TRUE;
}

LRESULT CALLBACK DialogProc(const HWND hWnd,
			    const UINT Msg, 
			    const WPARAM wParam,
			    const LPARAM lParam) 
{   
    LRESULT res = 0;

    switch (Msg) {
    case WM_CREATE:
	g_hWndMain = hWnd;
	if (FALSE == InitDialog()){
	    DestroyWindow(g_hWndMain);
	}
	break;

    case WM_CLOSE:
	DestroyWindow(g_hWndMain);
	break;

    case WM_DESTROY:
	if (g_hWndMenuBar)
	    DestroyWindow(g_hWndMenuBar);
	g_hWndMenuBar = NULL;
	g_hWndMain = NULL;
        PostQuitMessage(0);
        break;

    case WM_HOTKEY:
	/* Exit app when back is pressed. */
	if (VK_TBACK == HIWORD(lParam) && (0 != (MOD_KEYUP & LOWORD(lParam)))) {
	    DestroyWindow(g_hWndMain);
	} else {
	    return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	break;

    case WM_COMMAND:
	res = handle_menu(LOWORD(wParam));
	break;

    default:
	return DefWindowProc(hWnd, Msg, wParam, lParam);
    }

    return res;
}


/* === API === */

pj_status_t gui_init(gui_menu *menu)
{
    WNDCLASS wc;
    HWND hWnd = NULL;	
    RECT r;
    DWORD dwStyle;

    pj_status_t status  = PJ_SUCCESS;
    
    /* Check if app is running. If it's running then focus on the window */
    hWnd = FindWindow(MAINWINDOWCLASS, MAINWINDOWTITLE);

    if (NULL != hWnd) {
	SetForegroundWindow(hWnd);    
	return status;
    }

    g_menu = menu;

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)DialogProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInst;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName	= 0;
    wc.lpszClassName = MAINWINDOWCLASS;
    
    if (!RegisterClass(&wc) != 0) {
	DWORD err = GetLastError();
	return PJ_RETURN_OS_ERROR(err);
    }

    /* Create the app. window */
    g_hWndMain = CreateWindow(MAINWINDOWCLASS, MAINWINDOWTITLE,
			      WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 
			      CW_USEDEFAULT, CW_USEDEFAULT,
			      (HWND)NULL, NULL, g_hInst, (LPSTR)NULL);

    /* Create edit control to print log */
    GetClientRect(g_hWndMain, &r);
    dwStyle = WS_CHILD | WS_VISIBLE | WS_VSCROLL |
	      ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_LEFT;
    g_hWndLog = CreateWindow(
                TEXT("EDIT"),   // Class name
                NULL,           // Window text
                dwStyle,        // Window style
                0,		// x-coordinate of the upper-left corner
                0,              // y-coordinate of the upper-left corner
		r.right-r.left, // Width of the window for the edit
                                // control
		r.bottom-r.top, // Height of the window for the edit
                                // control
                g_hWndMain,     // Window handle to the parent window
                (HMENU) 0,	// Control identifier
                g_hInst,        // Instance handle
                NULL);          // Specify NULL for this parameter when 
                                // you create a control

    /* Resize the log */
    if (g_hWndMenuBar) {
	RECT r_menu = {0};

	GetWindowRect(g_hWndLog, &r);
	GetWindowRect(g_hWndMenuBar, &r_menu);
	if (r.bottom > r_menu.top) {
	    MoveWindow(g_hWndLog, 0, 0, r.right-r.left, 
		       (r.bottom-r.top)-(r_menu.bottom-r_menu.top), TRUE);
	}
    }

    /* Focus it, so SP user can scroll the log */
    SetFocus(g_hWndLog);

    /* Get the log thread */
    g_log_thread = pj_thread_this();

    /* Redirect log & update log decor setting */
    /*
    log_decor = pj_log_get_decor();
    log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR;
    pj_log_set_decor(log_decor);
    */
    g_log_writer_orig = pj_log_get_log_func();
    pj_log_set_log_func(&log_writer);

    return status;
}

static pj_status_t gui_update_menu(gui_menu *menu)
{
    enum { MENU_ID_START = 50000 };
    UINT id_start = MENU_ID_START;
    HMENU hRootMenu;
    SHMENUBARINFO mbi;

    /* delete existing menu */
    if (g_hWndMenuBar) {
	DestroyWindow(g_hWndMenuBar);
	g_hWndMenuBar = NULL;
    }

    /* delete menu handler map */
    g_menu_handler_cnt = 0;

    /* smartphone can only have two root menus */
    pj_assert(menu->submenu_cnt <= 2);

    /* generate menu tree */
    hRootMenu = CreateMenu();
    id_start = generate_submenu(hRootMenu, id_start, menu);

    /* initialize menubar */
    ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
    mbi.cbSize      = sizeof(SHMENUBARINFO);
    mbi.hwndParent  = g_hWndMain;
    mbi.dwFlags	    = SHCMBF_HIDESIPBUTTON|SHCMBF_HMENU;
    mbi.nToolBarId  = (UINT)hRootMenu;
    mbi.hInstRes    = g_hInst;

    if (FALSE == SHCreateMenuBar(&mbi)) {
	DWORD err = GetLastError();
        return PJ_RETURN_OS_ERROR(err);
    }

    /* store menu window handle */
    g_hWndMenuBar = mbi.hwndMB;

    /* store current menu */
    g_menu = menu;

    /* show the menu */
    DrawMenuBar(g_hWndMain);
    ShowWindow(g_hWndMenuBar, SW_SHOW);

    /* override back button */
    SendMessage(g_hWndMenuBar, SHCMBM_OVERRIDEKEY, VK_TBACK,
	    MAKELPARAM(SHMBOF_NODEFAULT | SHMBOF_NOTIFY,
	    SHMBOF_NODEFAULT | SHMBOF_NOTIFY));


    return PJ_SUCCESS;
}

enum gui_key gui_msgbox(const char *title, const char *message, enum gui_flag flag)
{
    wchar_t buf_title[64];
    wchar_t buf_msg[512];
    UINT wflag = 0;
    int retcode;

    pj_ansi_to_unicode(title, pj_ansi_strlen(title), buf_title, 64);
    pj_ansi_to_unicode(message, pj_ansi_strlen(message), buf_msg, 512);

    switch (flag) {
    case WITH_OK:
	wflag = MB_OK;
	break;
    case WITH_YESNO:
	wflag = MB_YESNO;
	break;
    case WITH_OKCANCEL:
	wflag = MB_OKCANCEL;
	break;
    }

    retcode = MessageBox(g_hWndMain, buf_msg, buf_title, wflag);

    switch (retcode) {
    case IDOK:
	return KEY_OK;
    case IDYES:
	return KEY_YES;
    case IDNO:
	return KEY_NO;
    default:
	return KEY_CANCEL;
    }
}

void gui_sleep(unsigned sec)
{
    pj_thread_sleep(sec * 1000);
}

pj_status_t gui_start(gui_menu *menu)
{
    MSG msg;

    PJ_UNUSED_ARG(menu);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (msg.wParam);
}

void gui_destroy(void)
{
    if (g_hWndMain) {
	DestroyWindow(g_hWndMain);
	g_hWndMain = NULL;
    }
}


int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nShowCmd
)
{
    int status;

    PJ_UNUSED_ARG(hPrevInstance);
    PJ_UNUSED_ARG(lpCmdLine);
    PJ_UNUSED_ARG(nShowCmd);

    // store the hInstance in global
    g_hInst = hInstance;

    status = systest_init();
    if (status != 0)
	goto on_return;

    status = systest_run();
	
on_return:
    systest_deinit();

    return status;
}

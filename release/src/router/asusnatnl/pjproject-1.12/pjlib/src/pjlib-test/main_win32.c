/* $Id: main_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include "test.h"

#include <pj/string.h>
#include <pj/unicode.h>
#include <pj/sock.h>
#include <pj/log.h>

#define WIN32_LEAN_AND_MEAN
#define NONAMELESSUNION
#include <windows.h>
#include <commctrl.h>
#include <tchar.h>

#define MAX_LOADSTRING	    100
#define THIS_FILE	    "main_win32.c"

#define IDC_HELLO_WINCE	    3
#define ID_LOGWINDOW	    104


ATOM			    MyRegisterClass	(HINSTANCE, LPTSTR);
BOOL			    InitInstance	(HINSTANCE, int);
LRESULT CALLBACK	    WndProc		(HWND, UINT, WPARAM, LPARAM);


extern int		    param_log_decor;	// in test.c

static HINSTANCE	    hInst;
static HWND		    hwndLog;
static HFONT		    hFixedFont;


static void write_log(int level, const char *data, int len)
{
    PJ_DECL_UNICODE_TEMP_BUF(wdata,256);

    PJ_UNUSED_ARG(level);
    PJ_UNUSED_ARG(len);
    SendMessage(hwndLog, EM_REPLACESEL, FALSE, 
		(LPARAM)PJ_STRING_TO_NATIVE(data,wdata,256));
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
		   LPTSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    
    PJ_UNUSED_ARG(lpCmdLine);
    PJ_UNUSED_ARG(hPrevInstance);

    
    if (!InitInstance (hInstance, nCmdShow))
	return FALSE;
    
    pj_log_set_log_func( &write_log );
    param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_CR;

    // Run the test!
    test_main();

    PJ_LOG(3,(THIS_FILE,""));
    PJ_LOG(3,(THIS_FILE,"Press ESC to quit"));

    // Message loop, waiting to quit.
    while (GetMessage(&msg, NULL, 0, 0)) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
    
    DeleteObject(hFixedFont);
    return msg.wParam;
}


#ifdef _CONSOLE
int main()
{
    return WinMain(GetModuleHandle(NULL), NULL, NULL, SW_SHOW);
}
#endif


ATOM MyRegisterClass(HINSTANCE hInstance, LPTSTR szWindowClass)
{
    WNDCLASS	wc;
    
    wc.style		= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc	= (WNDPROC) WndProc;
    wc.cbClsExtra	= 0;
    wc.cbWndExtra	= 0;
    wc.hInstance	= hInstance;
    ///wc.hIcon		= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLO_WINCE));
    wc.hIcon		= NULL;
    wc.hCursor		= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName	= 0;
    wc.lpszClassName	= szWindowClass;
    
    return RegisterClass(&wc);
}


BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    HWND	hWnd;
    TCHAR	*szTitle = _T("PJSIP Test");
    TCHAR	*szWindowClass = _T("PJSIP_TEST");
    LOGFONT	lf;


    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = 13;
#if PJ_NATIVE_STRING_IS_UNICODE
    wcscpy(lf.lfFaceName, _T("Courier New"));
#else
    strcpy(lf.lfFaceName, "Lucida Console");
#endif

    hFixedFont = CreateFontIndirect(&lf);
    if (!hFixedFont)
	return FALSE;

    hInst = hInstance;
    
    MyRegisterClass(hInstance, szWindowClass);
    
    hWnd = CreateWindow(szWindowClass, szTitle, WS_VISIBLE,
		        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
			CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    
    if (!hWnd)
	return FALSE;
    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    if (hwndLog) {
	SendMessage(hwndLog, WM_SETFONT, (WPARAM) hFixedFont, (LPARAM) 0);  
	ShowWindow(hwndLog, TRUE);
    }

    return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RECT rt;
    DWORD dwStyle;
    
    switch (message) 
    {
    case WM_CREATE:
	// Create text control.
	GetClientRect(hWnd, &rt);
	dwStyle = WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | 
		  WS_BORDER | ES_LEFT | ES_MULTILINE | ES_NOHIDESEL |
		  ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY; 
	hwndLog = CreateWindow( TEXT("edit"),	    // class
				NULL,		    // window text
				dwStyle,	    // style
				0,		    // x-left
				0,		    // y-top
				rt.right-rt.left,   // w
				rt.bottom-rt.top,   // h
				hWnd,		    // parent
				(HMENU)ID_LOGWINDOW,// id
				hInst,		    // instance
				NULL);		    // NULL for control.
	break;
    case WM_ACTIVATE:
	if (LOWORD(wParam) == WA_INACTIVE)
	    DestroyWindow(hWnd);
	break;
    case WM_CHAR:
	if (wParam == 27) {
	    DestroyWindow(hWnd);
	}
	break;
    case WM_CLOSE:
	DestroyWindow(hWnd);
	break;
    case WM_DESTROY:
	PostQuitMessage(0);
	break;
    default:
	return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


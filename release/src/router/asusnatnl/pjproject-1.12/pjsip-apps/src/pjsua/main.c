/* $Id: main.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsua-lib/pjsua.h>

#define THIS_FILE	"main.c"


/*
 * These are defined in pjsua_app.c.
 */
extern pj_bool_t app_restart;
pj_status_t app_init(int argc, char *argv[]);
pj_status_t app_main(void);
pj_status_t app_destroy(void);


#if defined(PJ_WIN32) && PJ_WIN32!=0
#include <windows.h>

static pj_thread_desc handler_desc;

static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    pj_thread_t *thread;

    switch (fdwCtrlType) 
    { 
        // Handle the CTRL+C signal. 
 
        case CTRL_C_EVENT: 
        case CTRL_CLOSE_EVENT: 
        case CTRL_BREAK_EVENT: 
        case CTRL_LOGOFF_EVENT: 
        case CTRL_SHUTDOWN_EVENT: 
	    pj_thread_register("ctrlhandler", handler_desc, &thread);
	    PJ_LOG(3,(THIS_FILE, "Ctrl-C detected, quitting.."));
            app_destroy();
	    ExitProcess(1);
            PJ_UNREACHED(return TRUE;)
 
        default: 
 
            return FALSE; 
    } 
}

static void setup_signal_handler(void)
{
    SetConsoleCtrlHandler(&CtrlHandler, TRUE);
}

static void setup_socket_signal()
{
}

#else
#include <signal.h>

static void setup_signal_handler(void)
{
}

static void setup_socket_signal()
{
    signal(SIGPIPE, SIG_IGN);
}

#endif

int main(int argc, char *argv[])
{
    setup_socket_signal();

    do {
	app_restart = PJ_FALSE;

	if (app_init(argc, argv) != PJ_SUCCESS)
	    return 1;

	setup_signal_handler();

	app_main();
	app_destroy();

	/* This is on purpose */
	app_destroy();
    } while (app_restart);

    return 0;
}


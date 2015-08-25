/* $Id: wince_main.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <windows.h>
#include <stdio.h>

#define TITLE	"PJMEDIA Test"
#define CAPTION	"This will start pjmedia test. Please do not use the PDA while the test is in progress. The test may take couple of minutes to complete, and you will be notified again when it completes"

static FILE *fLog;

static void log_writer_cb(int level, const char *data, int len)
{
    PJ_UNUSED_ARG(level);

    fwrite(data, len, 1, fLog);
}


int WINAPI WinMain(HINSTANCE hInstance,
		   HINSTANCE hPrevInstance,
		   LPTSTR    lpCmdLine,
		   int       nCmdShow)
{
    int rc;

    PJ_UNUSED_ARG(hInstance);
    PJ_UNUSED_ARG(hPrevInstance);
    PJ_UNUSED_ARG(lpCmdLine);
    PJ_UNUSED_ARG(nCmdShow);

    rc = MessageBox(0, TEXT(CAPTION), TEXT(TITLE), MB_OKCANCEL);
    if (rc != IDOK)
	return TRUE;

    fLog = fopen("\\pjmedia-test.txt", "wt");
    if (fLog == NULL) {
	MessageBox(0, TEXT("Unable to create result file"), TEXT(TITLE), MB_OK);
	return TRUE;
    }
    pj_log_set_log_func(&log_writer_cb);
    rc = test_main();

    fclose(fLog);

    if (rc != 0) {
	MessageBox(0, TEXT("Test failed"), TEXT(TITLE), MB_OK);
	return TRUE;
    }

    MessageBox(0, TEXT("Test has been successful. Please see the result in \"\\pjmedia-test.txt\" file"),
		  TEXT(TITLE), 0);
    return TRUE;
}


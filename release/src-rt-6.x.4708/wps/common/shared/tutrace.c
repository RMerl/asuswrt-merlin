/*
 * Debug messages
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tutrace.c 383924 2013-02-08 04:14:39Z $
 */

#if defined(WIN32) || defined(WINCE)
#include <windows.h>
#endif
#if !defined(WIN32)
#include <stdarg.h>
#endif
#include <stdio.h>
#if defined(__linux__) || defined(__ECOS)
#include <time.h>
#endif
#include "tutrace.h"

static WPS_TRACEMSG_OUTPUT_FN traceMsgOutputFn = NULL;

#ifdef _TUDEBUGTRACE
static unsigned int wps_msglevel = TUTRACELEVEL;
#else
static unsigned int wps_msglevel = 0;
#endif

void
wps_set_traceMsg_output_fn(WPS_TRACEMSG_OUTPUT_FN fn)
{
	traceMsgOutputFn = fn;
}

int
wps_tutrace_get_msglevel()
{
	return wps_msglevel;
}

void
wps_tutrace_set_msglevel(int level)
{
	wps_msglevel = level;
}

#ifdef _TUDEBUGTRACE
void
print_traceMsg(int level, const char *lpszFile,
                   int nLine, char *lpszFormat, ...)
{
	char szTraceMsg[2000];
	int cbMsg;
	va_list lpArgv;

#ifdef _WIN32_WCE
	TCHAR szMsgW[2000];
#endif

	if (!(TUTRACELEVEL & level)) {
		return;
	}

	/* Format trace msg prefix */
	if (traceMsgOutputFn != NULL)
		cbMsg = sprintf(szTraceMsg, "WPS: %s(%d):", lpszFile, nLine);
	else
		cbMsg = sprintf(szTraceMsg, "%s(%d):", lpszFile, nLine);


	/* Append trace msg to prefix. */
	va_start(lpArgv, lpszFormat);
	cbMsg = vsprintf(szTraceMsg + cbMsg, lpszFormat, lpArgv);
	va_end(lpArgv);

	if (traceMsgOutputFn != NULL) {
		traceMsgOutputFn(((TUTRACELEVEL & TUERR) != 0), szTraceMsg);
	} else {
#ifndef _WIN32_WCE
	#ifdef WIN32
		OutputDebugString(szTraceMsg);
	#else /* Linux */
		if (level & wps_msglevel)
			fprintf(stdout, "%s", szTraceMsg);
	#endif /* WIN32 */
#else /* _WIN32_WCE */
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szTraceMsg, -1,
			szMsgW, strlen(szTraceMsg)+1);
		RETAILMSG(1, (szMsgW));
#endif /* _WIN32_WCE */
	}
}
#endif /* _TUDEBUGTRACE */

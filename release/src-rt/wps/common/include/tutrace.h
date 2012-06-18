/*
 * TuTrace
 *
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tutrace.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _TUTRACE_H
#define _TUTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WPS_TRACEMSG_OUTPUT_FN)(int is_err, char *traceMsg);
void wps_set_traceMsg_output_fn(WPS_TRACEMSG_OUTPUT_FN fn);


#ifdef _TUDEBUGTRACE

#define TUTRACELEVEL    (TUINFO | TUERR)

/* trace levels */
#define TUINFO  0x0001
#define TUERR   0x0010

#define TUTRACE_ERR        TUERR, __FUNCTION__, __LINE__
#define TUTRACE_INFO       TUINFO, __FUNCTION__, __LINE__

#define TUTRACE(VARGLST)   print_traceMsg VARGLST

void print_traceMsg(int level, const char *lpszFile,
	int nLine, char *lpszFormat, ...);

#else

#define TUTRACE(VARGLST)    ((void)0)

#endif /* _TUDEBUGTRACE */

#ifdef __cplusplus
}
#endif

#endif /* _TUTRACE_H */

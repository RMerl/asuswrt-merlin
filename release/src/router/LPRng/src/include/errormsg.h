/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: errormsg.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/

#ifndef _ERRORMSG_H_
#define _ERRORMSG_H_ 1

#if defined(FORMAT_TEST)
#define LOGMSG(X) printf(
#define FATAL(X) printf(
#define LOGERR(X) printf(
#define LOGERR_DIE(X) printf(
#define LOGDEBUG printf
#define DIEMSG printf
#define WARNMSG printf
#define MESSAGE printf
#else
#define LOGMSG(X) logmsg(X,
#define FATAL(X) fatal(X,
#define LOGERR(X) logerr(X,
#define LOGERR_DIE(X) logerr_die(X,
#define LOGDEBUG logDebug
#define DIEMSG Diemsg
#define WARNMSG Warnmsg
#define MESSAGE Message
#endif

/* PROTOTYPES */
const char * Errormsg ( int err );
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logmsg(int kind, char *msg,...)
#else
 void logmsg(va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void fatal (int kind, char *msg,...)
#else
 void fatal (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logerr (int kind, char *msg,...)
#else
 void logerr (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logerr_die (int kind, char *msg,...)
#else
 void logerr_die (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Diemsg (char *msg,...)
#else
 void Diemsg (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Warnmsg (char *msg,...)
#else
 void Warnmsg (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Message (char *msg,...)
#else
 void Message (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void logDebug (char *msg,...)
#else
 void logDebug (va_alist) va_dcl
#endif
;
const char *Sigstr (int n);
const char *Decode_status (plp_status_t *status);
char *Server_status( int d );
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setstatus (struct job *job,char *fmt,...)
#else
 void setstatus (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setmessage (struct job *job,const char *header, char *fmt,...)
#else
 void setmessage (va_alist) va_dcl
#endif
;

#endif

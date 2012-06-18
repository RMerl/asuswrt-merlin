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
 *
 ***************************************************************************/

 static char *const _id =
"$Id: copyright.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
#include "patchlevel.h"
/**** ENDINCLUDE ****/

char *Copyright[] = {
 PATCHLEVEL
#if defined(KERBEROS)
 ", Kerberos5"
#endif
#if defined(MIT_KERBEROS4)
 ", MIT Kerberos4"
#endif
", Copyright 1988-2003 Patrick Powell, <papowell@lprng.com>",

"",
"locking uses: "
#ifdef HAVE_FCNTL
		"fcntl (preferred)"
#else
#ifdef HAVE_LOCKF
            "lockf"
#else
            "flock (does NOT work over NFS)"
#endif
#endif
,
"stty uses: "
#if USE_STTY == SGTTYB
            "sgttyb"
#endif
#if USE_STTY == TERMIO
            "termio"
#endif
#if USE_STTY == TERMIOS
            "termios"
#endif
,
#ifdef SSL_ENABLE
"with SSL"
#else
"without SSL"
#endif
,
"",
#include "license.h"
#include "copyright.h"
0 };

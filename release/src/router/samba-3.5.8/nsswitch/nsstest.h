/*
   Unix SMB/CIFS implementation.
   nss includes for the nss tester
   Copyright (C) Kai Blin 2007

     ** NOTE! The following LGPL license applies to the nsstest
     ** header. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _NSSTEST_H
#define _NSSTEST_H

#include <pwd.h>
#include <grp.h>

#ifdef HAVE_NSS_COMMON_H

/*
 * Sun Solaris
 */

#include <nss_common.h>
#include <nss_dbdefs.h>
#include <nsswitch.h>

typedef nss_status_t NSS_STATUS;

#define NSS_STATUS_SUCCESS     NSS_SUCCESS
#define NSS_STATUS_NOTFOUND    NSS_NOTFOUND
#define NSS_STATUS_UNAVAIL     NSS_UNAVAIL
#define NSS_STATUS_TRYAGAIN    NSS_TRYAGAIN

#elif HAVE_NSS_H

/*
 * Linux (glibc)
 */

#include <nss.h>
typedef enum nss_status NSS_STATUS;

#elif HAVE_NS_API_H

/*
 * SGI IRIX
 */

#ifdef DATUM
#define _DATUM_DEFINED
#endif

#include <ns_api.h>

typedef enum
{
	NSS_STATUS_SUCCESS=NS_SUCCESS,
		NSS_STATUS_NOTFOUND=NS_NOTFOUND,
		NSS_STATUS_UNAVAIL=NS_UNAVAIL,
		NSS_STATUS_TRYAGAIN=NS_TRYAGAIN
} NSS_STATUS;

#define NSD_MEM_STATIC 0
#define NSD_MEM_VOLATILE 1
#define NSD_MEM_DYNAMIC 2

#elif defined(HPUX) && defined(HAVE_NSSWITCH_H)

/* HP-UX 11 */

#include <nsswitch.h>

#define NSS_STATUS_SUCCESS     NSS_SUCCESS
#define NSS_STATUS_NOTFOUND    NSS_NOTFOUND
#define NSS_STATUS_UNAVAIL     NSS_UNAVAIL
#define NSS_STATUS_TRYAGAIN    NSS_TRYAGAIN

#ifdef HAVE_SYNCH_H
#include <synch.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

typedef enum {
	NSS_SUCCESS,
	NSS_NOTFOUND,
	NSS_UNAVAIL,
	NSS_TRYAGAIN
} nss_status_t;

typedef nss_status_t NSS_STATUS;

#else /* Nothing's defined. Neither solaris nor gnu nor sun nor hp */

typedef enum
{
	NSS_STATUS_SUCCESS=0,
	NSS_STATUS_NOTFOUND=1,
	NSS_STATUS_UNAVAIL=2,
	NSS_STATUS_TRYAGAIN=3
} NSS_STATUS;

#endif

#endif /* _NSSTEST_H */

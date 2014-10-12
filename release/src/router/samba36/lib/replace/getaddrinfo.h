/*
PostgreSQL Database Management System
(formerly known as Postgres, then as Postgres95)

Portions Copyright (c) 1996-2005, The PostgreSQL Global Development Group

Portions Copyright (c) 1994, The Regents of the University of California

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement
is hereby granted, provided that the above copyright notice and this paragraph
and the following two paragraphs appear in all copies.

IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS
ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS
TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*/

/*-------------------------------------------------------------------------
 *
 * getaddrinfo.h
 *	  Support getaddrinfo() on platforms that don't have it.
 *
 * Note: we use our own routines on platforms that don't HAVE_STRUCT_ADDRINFO,
 * whether or not the library routine getaddrinfo() can be found.  This
 * policy is needed because on some platforms a manually installed libbind.a
 * may provide getaddrinfo(), yet the system headers may not provide the
 * struct definitions needed to call it.  To avoid conflict with the libbind
 * definition in such cases, we rename our routines to pg_xxx() via macros.
 *

in lib/replace we use rep_xxx()

 * This code will also work on platforms where struct addrinfo is defined
 * in the system headers but no getaddrinfo() can be located.
 *
 * Copyright (c) 2003-2007, PostgreSQL Global Development Group
 *
 *-------------------------------------------------------------------------
 */
#ifndef GETADDRINFO_H
#define GETADDRINFO_H

#ifndef HAVE_GETADDRINFO

/* Rename private copies per comments above */
#ifdef getaddrinfo
#undef getaddrinfo
#endif
#define getaddrinfo rep_getaddrinfo
#define HAVE_GETADDRINFO

#ifdef freeaddrinfo
#undef freeaddrinfo
#endif
#define freeaddrinfo rep_freeaddrinfo
#define HAVE_FREEADDRINFO

#ifdef gai_strerror
#undef gai_strerror
#endif
#define gai_strerror rep_gai_strerror
#define HAVE_GAI_STRERROR

#ifdef getnameinfo
#undef getnameinfo
#endif
#define getnameinfo rep_getnameinfo
#ifndef HAVE_GETNAMEINFO
#define HAVE_GETNAMEINFO
#endif

extern int rep_getaddrinfo(const char *node, const char *service,
			const struct addrinfo * hints, struct addrinfo ** res);
extern void rep_freeaddrinfo(struct addrinfo * res);
extern const char *rep_gai_strerror(int errcode);
extern int rep_getnameinfo(const struct sockaddr * sa, socklen_t salen,
			char *node, size_t nodelen,
			char *service, size_t servicelen, int flags);
#endif   /* HAVE_GETADDRINFO */

#endif   /* GETADDRINFO_H */

/*
   Unix SMB/CIFS implementation.

   Winbind daemon for ntdom nss module

   Copyright (C) Tim Potter 2000

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

#ifndef _WINBIND_NSS_SOLARIS_H
#define _WINBIND_NSS_SOLARIS_H

#include <nss_common.h>
#include <nss_dbdefs.h>
#include <nsswitch.h>

typedef nss_status_t NSS_STATUS;

#define NSS_STATUS_SUCCESS     NSS_SUCCESS
#define NSS_STATUS_NOTFOUND    NSS_NOTFOUND
#define NSS_STATUS_UNAVAIL     NSS_UNAVAIL
#define NSS_STATUS_TRYAGAIN    NSS_TRYAGAIN

/* The solaris winbind is implemented as a wrapper around the linux
   version. */

NSS_STATUS _nss_winbind_setpwent(void);
NSS_STATUS _nss_winbind_endpwent(void);
NSS_STATUS _nss_winbind_getpwent_r(struct passwd* result, char* buffer,
				   size_t buflen, int* errnop);
NSS_STATUS _nss_winbind_getpwuid_r(uid_t, struct passwd*, char* buffer,
				   size_t buflen, int* errnop);
NSS_STATUS _nss_winbind_getpwnam_r(const char* name, struct passwd* result,
				   char* buffer, size_t buflen, int* errnop);

NSS_STATUS _nss_winbind_setgrent(void);
NSS_STATUS _nss_winbind_endgrent(void);
NSS_STATUS _nss_winbind_getgrent_r(struct group* result, char* buffer,
				   size_t buflen, int* errnop);
NSS_STATUS _nss_winbind_getgrnam_r(const char *name,
				   struct group *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getgrgid_r(gid_t gid,
				   struct group *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_initgroups_dyn(char *user, gid_t group, long int *start,
				       long int *size, gid_t **groups,
				       long int limit, int *errnop);

#endif /* _WINBIND_NSS_SOLARIS_H */

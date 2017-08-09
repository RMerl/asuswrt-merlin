/*
   Unix SMB/CIFS implementation.

   NetBSD loadable authentication module, providing identification
   routines against Samba winbind/Windows NT Domain

   Copyright (C) Luke Mewburn 2004-2005

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/


#include "winbind_client.h"

#include <sys/param.h>
#include <stdarg.h>
#include <syslog.h>

	/* dynamic nsswitch with "new" getpw* nsdispatch API available */
#if defined(NSS_MODULE_INTERFACE_VERSION) && defined(HAVE_GETPWENT_R)

/*
	group functions
	---------------
*/

static struct group	_winbind_group;
static char		_winbind_groupbuf[1024];

/*
 * We need a proper prototype for this :-)
 */

NSS_STATUS _nss_winbind_setpwent(void);
NSS_STATUS _nss_winbind_endpwent(void);
NSS_STATUS _nss_winbind_getpwent_r(struct passwd *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getpwuid_r(uid_t uid, struct passwd *result,
				   char *buffer, size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getpwnam_r(const char *name, struct passwd *result,
				   char *buffer, size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_setgrent(void);
NSS_STATUS _nss_winbind_endgrent(void);
NSS_STATUS _nss_winbind_getgrent_r(struct group *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getgrlst_r(struct group *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getgrnam_r(const char *name, struct group *result,
				   char *buffer, size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_getgrgid_r(gid_t gid, struct group *result, char *buffer,
				   size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_initgroups_dyn(char *user, gid_t group, long int *start,
				       long int *size, gid_t **groups,
				       long int limit, int *errnop);
NSS_STATUS _nss_winbind_getusersids(const char *user_sid, char **group_sids,
				    int *num_groups, char *buffer, size_t buf_size,
				    int *errnop);
NSS_STATUS _nss_winbind_nametosid(const char *name, char **sid, char *buffer,
				  size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_sidtoname(const char *sid, char **name, char *buffer,
				  size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_sidtouid(const char *sid, uid_t *uid, int *errnop);
NSS_STATUS _nss_winbind_sidtogid(const char *sid, gid_t *gid, int *errnop);
NSS_STATUS _nss_winbind_uidtosid(uid_t uid, char **sid, char *buffer,
				 size_t buflen, int *errnop);
NSS_STATUS _nss_winbind_gidtosid(gid_t gid, char **sid, char *buffer,
				 size_t buflen, int *errnop);

int
netbsdwinbind_endgrent(void *nsrv, void *nscb, va_list ap)
{
	int	rv;

	rv = _nss_winbind_endgrent();
	return rv;
}

int
netbsdwinbind_setgrent(void *nsrv, void *nscb, va_list ap)
{
	int	rv;

	rv = _nss_winbind_setgrent();
	return rv;
}

int
netbsdwinbind_getgrent(void *nsrv, void *nscb, va_list ap)
{
	struct group   **retval	= va_arg(ap, struct group **);

	int	rv, rerrno;

	*retval = NULL;
	rv = _nss_winbind_getgrent_r(&_winbind_group,
	    _winbind_groupbuf, sizeof(_winbind_groupbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_group;
	return rv;
}

int
netbsdwinbind_getgrent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getgrent_r(grp, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*retval = rerrno;
	return rv;
}

int
netbsdwinbind_getgrgid(void *nsrv, void *nscb, va_list ap)
{
	struct group   **retval	= va_arg(ap, struct group **);
	gid_t		 gid	= va_arg(ap, gid_t);

	int	rv, rerrno;

	*retval = NULL;
	rv = _nss_winbind_getgrgid_r(gid, &_winbind_group,
	    _winbind_groupbuf, sizeof(_winbind_groupbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_group;
	return rv;
}

int
netbsdwinbind_getgrgid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	gid_t		 gid	= va_arg(ap, gid_t);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getgrgid_r(gid, grp, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*retval = rerrno;
	return rv;
}

int
netbsdwinbind_getgrnam(void *nsrv, void *nscb, va_list ap)
{
	struct group   **retval	= va_arg(ap, struct group **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerrno;

	*retval = NULL;
	rv = _nss_winbind_getgrnam_r(name, &_winbind_group,
	    _winbind_groupbuf, sizeof(_winbind_groupbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_group;
	return rv;
}

int
netbsdwinbind_getgrnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct group	*grp	= va_arg(ap, struct group *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct group   **result	= va_arg(ap, struct group **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getgrnam_r(name, grp, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = grp;
	else
		*retval = rerrno;
	return rv;
}

int
netbsdwinbind_getgroupmembership(void *nsrv, void *nscb, va_list ap)
{
	int		*result	= va_arg(ap, int *);
	const char 	*uname	= va_arg(ap, const char *);
	gid_t		*groups	= va_arg(ap, gid_t *);
	int		 maxgrp	= va_arg(ap, int);
	int		*groupc	= va_arg(ap, int *);

	struct winbindd_request request;
	struct winbindd_response response;
	gid_t	*wblistv;
	int	wblistc, i, isdup, dupc;

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);
	strncpy(request.data.username, uname,
				sizeof(request.data.username) - 1);
	i = winbindd_request_response(WINBINDD_GETGROUPS, &request, &response);
	if (i != NSS_STATUS_SUCCESS)
		return NS_NOTFOUND;
	wblistv = (gid_t *)response.extra_data.data;
	wblistc = response.data.num_entries;

	for (i = 0; i < wblistc; i++) {			/* add winbind gids */
		isdup = 0;				/* skip duplicates */
		for (dupc = 0; dupc < MIN(maxgrp, *groupc); dupc++) {
			if (groups[dupc] == wblistv[i]) {
				isdup = 1;
				break;
			}
		}
		if (isdup)
			continue;
		if (*groupc < maxgrp)			/* add this gid */
			groups[*groupc] = wblistv[i];
		else
			*result = -1;
		(*groupc)++;
	}
	SAFE_FREE(wblistv);
	return NS_NOTFOUND;
}


/*
	passwd functions
	----------------
*/

static struct passwd	_winbind_passwd;
static char		_winbind_passwdbuf[1024];

int
netbsdwinbind_endpwent(void *nsrv, void *nscb, va_list ap)
{
	int	rv;

	rv = _nss_winbind_endpwent();
	return rv;
}

int
netbsdwinbind_setpwent(void *nsrv, void *nscb, va_list ap)
{
	int	rv;

	rv = _nss_winbind_setpwent();
	return rv;
}

int
netbsdwinbind_getpwent(void *nsrv, void *nscb, va_list ap)
{
	struct passwd  **retval	= va_arg(ap, struct passwd **);

	int	rv, rerrno;

	*retval = NULL;

	rv = _nss_winbind_getpwent_r(&_winbind_passwd,
	    _winbind_passwdbuf, sizeof(_winbind_passwdbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_passwd;
	return rv;
}

int
netbsdwinbind_getpwent_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getpwent_r(pw, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*retval = rerrno;
	return rv;
}

int
netbsdwinbind_getpwnam(void *nsrv, void *nscb, va_list ap)
{
	struct passwd  **retval	= va_arg(ap, struct passwd **);
	const char	*name	= va_arg(ap, const char *);

	int	rv, rerrno;

	*retval = NULL;
	rv = _nss_winbind_getpwnam_r(name, &_winbind_passwd,
	    _winbind_passwdbuf, sizeof(_winbind_passwdbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_passwd;
	return rv;
}

int
netbsdwinbind_getpwnam_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	const char	*name	= va_arg(ap, const char *);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getpwnam_r(name, pw, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*retval = rerrno;
	return rv;
}

int
netbsdwinbind_getpwuid(void *nsrv, void *nscb, va_list ap)
{
	struct passwd  **retval	= va_arg(ap, struct passwd **);
	uid_t		 uid	= va_arg(ap, uid_t);

	int	rv, rerrno;

	*retval = NULL;
	rv = _nss_winbind_getpwuid_r(uid, &_winbind_passwd,
	    _winbind_passwdbuf, sizeof(_winbind_passwdbuf), &rerrno);
	if (rv == NS_SUCCESS)
		*retval = &_winbind_passwd;
	return rv;
}

int
netbsdwinbind_getpwuid_r(void *nsrv, void *nscb, va_list ap)
{
	int		*retval = va_arg(ap, int *);
	uid_t		 uid	= va_arg(ap, uid_t);
	struct passwd	*pw	= va_arg(ap, struct passwd *);
	char		*buffer	= va_arg(ap, char *);
	size_t		 buflen	= va_arg(ap, size_t);
	struct passwd  **result	= va_arg(ap, struct passwd **);

	int	rv, rerrno;

	*result = NULL;
	rerrno = 0;

	rv = _nss_winbind_getpwuid_r(uid, pw, buffer, buflen, &rerrno);
	if (rv == NS_SUCCESS)
		*result = pw;
	else
		*retval = rerrno;
	return rv;
}


/*
	nsswitch module setup
	---------------------
*/


static ns_mtab winbind_methods[] = {

{ NSDB_GROUP, "endgrent",	netbsdwinbind_endgrent,		NULL },
{ NSDB_GROUP, "getgrent",	netbsdwinbind_getgrent,		NULL },
{ NSDB_GROUP, "getgrent_r",	netbsdwinbind_getgrent_r,	NULL },
{ NSDB_GROUP, "getgrgid",	netbsdwinbind_getgrgid,		NULL },
{ NSDB_GROUP, "getgrgid_r",	netbsdwinbind_getgrgid_r,	NULL },
{ NSDB_GROUP, "getgrnam",	netbsdwinbind_getgrnam,		NULL },
{ NSDB_GROUP, "getgrnam_r",	netbsdwinbind_getgrnam_r,	NULL },
{ NSDB_GROUP, "setgrent",	netbsdwinbind_setgrent,		NULL },
{ NSDB_GROUP, "setgroupent",	netbsdwinbind_setgrent,		NULL },
{ NSDB_GROUP, "getgroupmembership", netbsdwinbind_getgroupmembership, NULL },

{ NSDB_PASSWD, "endpwent",	netbsdwinbind_endpwent,		NULL },
{ NSDB_PASSWD, "getpwent",	netbsdwinbind_getpwent,		NULL },
{ NSDB_PASSWD, "getpwent_r",	netbsdwinbind_getpwent_r,	NULL },
{ NSDB_PASSWD, "getpwnam",	netbsdwinbind_getpwnam,		NULL },
{ NSDB_PASSWD, "getpwnam_r",	netbsdwinbind_getpwnam_r,	NULL },
{ NSDB_PASSWD, "getpwuid",	netbsdwinbind_getpwuid,		NULL },
{ NSDB_PASSWD, "getpwuid_r",	netbsdwinbind_getpwuid_r,	NULL },
{ NSDB_PASSWD, "setpassent",	netbsdwinbind_setpwent,		NULL },
{ NSDB_PASSWD, "setpwent",	netbsdwinbind_setpwent,		NULL },

};

ns_mtab *
nss_module_register(const char *source, unsigned int *mtabsize,
    nss_module_unregister_fn *unreg)
{
        *mtabsize = sizeof(winbind_methods)/sizeof(winbind_methods[0]);
        *unreg = NULL;
        return (winbind_methods);
}

#endif /* NSS_MODULE_INTERFACE_VERSION && HAVE_GETPWENT_R */

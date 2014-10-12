/* 
   Unix SMB/CIFS implementation.

   Safe versions of getpw* calls

   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison  1998-2005
   Copyright (C) Andrew Bartlett 2002
   Copyright (C) Timur Bakeyev        2005
   Copyright (C) Bjoern Jacke    2006-2007

   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "system/passwd.h"
#include "lib/util/util_pw.h"

/**************************************************************************
 Wrappers for setpwent(), getpwent() and endpwent()
****************************************************************************/

void sys_setpwent(void)
{
	setpwent();
}

struct passwd *sys_getpwent(void)
{
	return getpwent();
}

void sys_endpwent(void)
{
	endpwent();
}

/**************************************************************************
 Wrappers for getpwnam(), getpwuid(), getgrnam(), getgrgid()
****************************************************************************/

struct passwd *sys_getpwnam(const char *name)
{
	return getpwnam(name);
}

struct passwd *sys_getpwuid(uid_t uid)
{
	return getpwuid(uid);
}

struct group *sys_getgrnam(const char *name)
{
	return getgrnam(name);
}

struct group *sys_getgrgid(gid_t gid)
{
	return getgrgid(gid);
}

struct passwd *tcopy_passwd(TALLOC_CTX *mem_ctx,
			    const struct passwd *from)
{
	struct passwd *ret = talloc_zero(mem_ctx, struct passwd);

	if (ret == NULL)
		return NULL;

	ret->pw_name = talloc_strdup(ret, from->pw_name);
	ret->pw_passwd = talloc_strdup(ret, from->pw_passwd);
	ret->pw_uid = from->pw_uid;
	ret->pw_gid = from->pw_gid;
	ret->pw_gecos = talloc_strdup(ret, from->pw_gecos);
	ret->pw_dir = talloc_strdup(ret, from->pw_dir);
	ret->pw_shell = talloc_strdup(ret, from->pw_shell);

	return ret;
}

struct passwd *getpwnam_alloc(TALLOC_CTX *mem_ctx, const char *name)
{
	struct passwd *temp;

	temp = sys_getpwnam(name);
	
	if (!temp) {
#if 0
		if (errno == ENOMEM) {
			/* what now? */
		}
#endif
		return NULL;
	}

	return tcopy_passwd(mem_ctx, temp);
}

/****************************************************************************
 talloc'ed version of getpwuid.
****************************************************************************/

struct passwd *getpwuid_alloc(TALLOC_CTX *mem_ctx, uid_t uid)
{
	struct passwd *temp;

	temp = sys_getpwuid(uid);
	
	if (!temp) {
#if 0
		if (errno == ENOMEM) {
			/* what now? */
		}
#endif
		return NULL;
	}

	return tcopy_passwd(mem_ctx, temp);
}

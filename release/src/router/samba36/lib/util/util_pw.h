/*
   Unix SMB/CIFS implementation.

   Safe versions of getpw* calls

   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 1997-2001.
   Copyright (C) Andrew Bartlett 2002

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

#ifndef __LIB_UTIL_UTIL_PW_H__
#define __LIB_UTIL_UTIL_PW_H__

void sys_setpwent(void);
struct passwd *sys_getpwent(void);
void sys_endpwent(void);
struct passwd *sys_getpwnam(const char *name);
struct passwd *sys_getpwuid(uid_t uid);
struct group *sys_getgrnam(const char *name);
struct group *sys_getgrgid(gid_t gid);
struct passwd *tcopy_passwd(TALLOC_CTX *mem_ctx,
			    const struct passwd *from);
struct passwd *getpwnam_alloc(TALLOC_CTX *mem_ctx, const char *name);
struct passwd *getpwuid_alloc(TALLOC_CTX *mem_ctx, uid_t uid);

#endif /* __LIB_UTIL_UTIL_PW_H__ */

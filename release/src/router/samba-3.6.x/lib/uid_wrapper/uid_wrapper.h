/*
   Copyright (C) Andrew Tridgell 2009
 
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

#ifndef __UID_WRAPPER_H__
#define __UID_WRAPPER_H__
#ifndef uwrap_enabled

int uwrap_enabled(void);
int uwrap_seteuid(uid_t euid);
uid_t uwrap_geteuid(void);
int uwrap_setegid(gid_t egid);
uid_t uwrap_getegid(void);
int uwrap_setgroups(size_t size, const gid_t *list);
int uwrap_getgroups(int size, gid_t *list);
uid_t uwrap_getuid(void);
gid_t uwrap_getgid(void);

#ifdef seteuid
#undef seteuid
#endif
#define seteuid	uwrap_seteuid

#ifdef setegid
#undef setegid
#endif
#define setegid	uwrap_setegid

#ifdef geteuid
#undef geteuid
#endif
#define geteuid	uwrap_geteuid

#ifdef getegid
#undef getegid
#endif
#define getegid	uwrap_getegid

#ifdef setgroups
#undef setgroups
#endif
#define setgroups uwrap_setgroups

#ifdef getgroups
#undef getgroups
#endif
#define getgroups uwrap_getgroups

#ifdef getuid
#undef getuid
#endif
#define getuid	uwrap_getuid

#ifdef getgid
#undef getgid
#endif
#define getgid	uwrap_getgid

#endif
#endif /* __UID_WRAPPER_H__ */

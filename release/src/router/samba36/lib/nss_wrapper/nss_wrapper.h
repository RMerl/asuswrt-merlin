/*
 * Copyright (C) Stefan Metzmacher 2007 <metze@samba.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __NSS_WRAPPER_H__
#define __NSS_WRAPPER_H__

struct passwd *nwrap_getpwnam(const char *name);
int nwrap_getpwnam_r(const char *name, struct passwd *pwbuf,
		     char *buf, size_t buflen, struct passwd **pwbufp);
struct passwd *nwrap_getpwuid(uid_t uid);
int nwrap_getpwuid_r(uid_t uid, struct passwd *pwbuf,
		     char *buf, size_t buflen, struct passwd **pwbufp);
void nwrap_setpwent(void);
struct passwd *nwrap_getpwent(void);
int nwrap_getpwent_r(struct passwd *pwbuf, char *buf,
		     size_t buflen, struct passwd **pwbufp);
void nwrap_endpwent(void);
int nwrap_initgroups(const char *user, gid_t group);
int nwrap_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);
struct group *nwrap_getgrnam(const char *name);
int nwrap_getgrnam_r(const char *name, struct group *gbuf,
		     char *buf, size_t buflen, struct group **gbufp);
struct group *nwrap_getgrgid(gid_t gid);
int nwrap_getgrgid_r(gid_t gid, struct group *gbuf,
		     char *buf, size_t buflen, struct group **gbufp);
void nwrap_setgrent(void);
struct group *nwrap_getgrent(void);
int nwrap_getgrent_r(struct group *gbuf, char *buf,
		     size_t buflen, struct group **gbufp);
void nwrap_endgrent(void);

#ifdef NSS_WRAPPER_REPLACE

#ifdef getpwnam
#undef getpwnam
#endif
#define getpwnam	nwrap_getpwnam

#ifdef getpwnam_r
#undef getpwnam_r
#endif
#define getpwnam_r	nwrap_getpwnam_r

#ifdef getpwuid
#undef getpwuid
#endif
#define getpwuid	nwrap_getpwuid

#ifdef getpwuid_r
#undef getpwuid_r
#endif
#define getpwuid_r	nwrap_getpwuid_r

#ifdef setpwent
#undef setpwent
#endif
#define setpwent	nwrap_setpwent

#ifdef getpwent
#undef getpwent
#endif
#define getpwent	nwrap_getpwent

#ifdef getpwent_r
#undef getpwent_r
#endif
#define getpwent_r	nwrap_getpwent_r

#ifdef endpwent
#undef endpwent
#endif
#define endpwent	nwrap_endpwent

#ifdef getgrlst
#undef getgrlst
#endif
#define getgrlst	__none_nwrap_getgrlst

#ifdef getgrlst_r
#undef getgrlst_r
#endif
#define getgrlst_r	__none_nwrap_getgrlst_r

#ifdef initgroups_dyn
#undef initgroups_dyn
#endif
#define initgroups_dyn	__none_nwrap_initgroups_dyn

#ifdef initgroups
#undef initgroups
#endif
#define initgroups	nwrap_initgroups

#ifdef getgrouplist
#undef getgrouplist
#endif
#define getgrouplist	nwrap_getgrouplist

#ifdef getgrnam
#undef getgrnam
#endif
#define getgrnam	nwrap_getgrnam

#ifdef getgrnam_r
#undef getgrnam_r
#endif
#define getgrnam_r	nwrap_getgrnam_r

#ifdef getgrgid
#undef getgrgid
#endif
#define getgrgid	nwrap_getgrgid

#ifdef getgrgid_r
#undef getgrgid_r
#endif
#define getgrgid_r	nwrap_getgrgid_r

#ifdef setgrent
#undef setgrent
#endif
#define setgrent	nwrap_setgrent

#ifdef getgrent
#undef getgrent
#endif
#define getgrent	nwrap_getgrent

#ifdef getgrent_r
#undef getgrent_r
#endif
#define getgrent_r	nwrap_getgrent_r

#ifdef endgrent
#undef endgrent
#endif
#define endgrent	nwrap_endgrent

#endif /* NSS_WRAPPER_REPLACE */

#endif /* __NSS_WRAPPER_H__ */

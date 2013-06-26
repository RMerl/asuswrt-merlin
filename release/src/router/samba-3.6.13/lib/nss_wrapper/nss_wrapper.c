/*
 * Copyright (C) Stefan Metzmacher 2007 <metze@samba.org>
 * Copyright (C) Guenther Deschner 2009 <gd@samba.org>
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

#ifdef _SAMBA_BUILD_

/* defining this gives us the posix getpwnam_r() calls on solaris
   Thanks to heimdal for this */
#define _POSIX_PTHREAD_SEMANTICS

#define NSS_WRAPPER_NOT_REPLACE
#include "../replace/replace.h"
#include "system/passwd.h"
#include "system/filesys.h"
#include "../nsswitch/nsstest.h"

#else /* _SAMBA_BUILD_ */

#error nss_wrapper_only_supported_in_samba_yet

#endif

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

/* not all systems have _r functions... */
#ifndef HAVE_GETPWNAM_R
#define getpwnam_r(name, pwdst, buf, buflen, pwdstp)	ENOSYS
#endif
#ifndef HAVE_GETPWUID_R
#define getpwuid_r(uid, pwdst, buf, buflen, pwdstp)	ENOSYS
#endif
#ifndef HAVE_GETPWENT_R
#define getpwent_r(pwdst, buf, buflen, pwdstp)		ENOSYS
#endif
#ifndef HAVE_GETGRNAM_R
#define getgrnam_r(name, grdst, buf, buflen, grdstp)	ENOSYS
#endif
#ifndef HAVE_GETGRGID_R
#define getgrgid_r(gid, grdst, buf, buflen, grdstp)	ENOSYS
#endif
#ifndef HAVE_GETGRENT_R
#define getgrent_r(grdst, buf, buflen, grdstp)		ENOSYS
#endif

/* not all systems have getgrouplist */
#ifndef HAVE_GETGROUPLIST
#define getgrouplist(user, group, groups, ngroups)	0
#endif

/* LD_PRELOAD doesn't work yet, so REWRITE_CALLS is all we support
 * for now */
#define REWRITE_CALLS

#ifdef REWRITE_CALLS

#define real_getpwnam		getpwnam
#define real_getpwnam_r		getpwnam_r
#define real_getpwuid		getpwuid
#define real_getpwuid_r		getpwuid_r

#define real_setpwent		setpwent
#define real_getpwent		getpwent
#define real_getpwent_r		getpwent_r
#define real_endpwent		endpwent

/*
#define real_getgrlst		getgrlst
#define real_getgrlst_r		getgrlst_r
#define real_initgroups_dyn	initgroups_dyn
*/
#define real_initgroups		initgroups
#define real_getgrouplist	getgrouplist

#define real_getgrnam		getgrnam
#define real_getgrnam_r		getgrnam_r
#define real_getgrgid		getgrgid
#define real_getgrgid_r		getgrgid_r

#define real_setgrent		setgrent
#define real_getgrent		getgrent
#define real_getgrent_r		getgrent_r
#define real_endgrent		endgrent

#endif

#if 0
# ifdef DEBUG
# define NWRAP_ERROR(args)	DEBUG(0, args)
# else
# define NWRAP_ERROR(args)	printf args
# endif
#else
#define NWRAP_ERROR(args)
#endif

#if 0
# ifdef DEBUG
# define NWRAP_DEBUG(args)	DEBUG(0, args)
# else
# define NWRAP_DEBUG(args)	printf args
# endif
#else
#define NWRAP_DEBUG(args)
#endif

#if 0
# ifdef DEBUG
# define NWRAP_VERBOSE(args)	DEBUG(0, args)
# else
# define NWRAP_VERBOSE(args)	printf args
# endif
#else
#define NWRAP_VERBOSE(args)
#endif

struct nwrap_module_nss_fns {
	NSS_STATUS (*_nss_getpwnam_r)(const char *name, struct passwd *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_getpwuid_r)(uid_t uid, struct passwd *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_setpwent)(void);
	NSS_STATUS (*_nss_getpwent_r)(struct passwd *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_endpwent)(void);
	NSS_STATUS (*_nss_initgroups)(const char *user, gid_t group, long int *start,
				      long int *size, gid_t **groups, long int limit, int *errnop);
	NSS_STATUS (*_nss_getgrnam_r)(const char *name, struct group *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_getgrgid_r)(gid_t gid, struct group *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_setgrent)(void);
	NSS_STATUS (*_nss_getgrent_r)(struct group *result, char *buffer,
				      size_t buflen, int *errnop);
	NSS_STATUS (*_nss_endgrent)(void);
};

struct nwrap_backend {
	const char *name;
	const char *so_path;
	void *so_handle;
	struct nwrap_ops *ops;
	struct nwrap_module_nss_fns *fns;
};

struct nwrap_ops {
	struct passwd *	(*nw_getpwnam)(struct nwrap_backend *b,
				       const char *name);
	int		(*nw_getpwnam_r)(struct nwrap_backend *b,
					 const char *name, struct passwd *pwdst,
					 char *buf, size_t buflen, struct passwd **pwdstp);
	struct passwd *	(*nw_getpwuid)(struct nwrap_backend *b,
				       uid_t uid);
	int		(*nw_getpwuid_r)(struct nwrap_backend *b,
					 uid_t uid, struct passwd *pwdst,
					 char *buf, size_t buflen, struct passwd **pwdstp);
	void		(*nw_setpwent)(struct nwrap_backend *b);
	struct passwd *	(*nw_getpwent)(struct nwrap_backend *b);
	int		(*nw_getpwent_r)(struct nwrap_backend *b,
					 struct passwd *pwdst, char *buf,
					 size_t buflen, struct passwd **pwdstp);
	void		(*nw_endpwent)(struct nwrap_backend *b);
	int		(*nw_initgroups)(struct nwrap_backend *b,
					 const char *user, gid_t group);
	struct group *	(*nw_getgrnam)(struct nwrap_backend *b,
				       const char *name);
	int		(*nw_getgrnam_r)(struct nwrap_backend *b,
					 const char *name, struct group *grdst,
					 char *buf, size_t buflen, struct group **grdstp);
	struct group *	(*nw_getgrgid)(struct nwrap_backend *b,
				       gid_t gid);
	int		(*nw_getgrgid_r)(struct nwrap_backend *b,
					 gid_t gid, struct group *grdst,
					 char *buf, size_t buflen, struct group **grdstp);
	void		(*nw_setgrent)(struct nwrap_backend *b);
	struct group *	(*nw_getgrent)(struct nwrap_backend *b);
	int		(*nw_getgrent_r)(struct nwrap_backend *b,
					 struct group *grdst, char *buf,
					 size_t buflen, struct group **grdstp);
	void		(*nw_endgrent)(struct nwrap_backend *b);
};

/* prototypes for files backend */


static struct passwd *nwrap_files_getpwnam(struct nwrap_backend *b,
					   const char *name);
static int nwrap_files_getpwnam_r(struct nwrap_backend *b,
				  const char *name, struct passwd *pwdst,
				  char *buf, size_t buflen, struct passwd **pwdstp);
static struct passwd *nwrap_files_getpwuid(struct nwrap_backend *b,
					   uid_t uid);
static int nwrap_files_getpwuid_r(struct nwrap_backend *b,
				  uid_t uid, struct passwd *pwdst,
				  char *buf, size_t buflen, struct passwd **pwdstp);
static void nwrap_files_setpwent(struct nwrap_backend *b);
static struct passwd *nwrap_files_getpwent(struct nwrap_backend *b);
static int nwrap_files_getpwent_r(struct nwrap_backend *b,
				  struct passwd *pwdst, char *buf,
				  size_t buflen, struct passwd **pwdstp);
static void nwrap_files_endpwent(struct nwrap_backend *b);
static int nwrap_files_initgroups(struct nwrap_backend *b,
				  const char *user, gid_t group);
static struct group *nwrap_files_getgrnam(struct nwrap_backend *b,
					  const char *name);
static int nwrap_files_getgrnam_r(struct nwrap_backend *b,
				  const char *name, struct group *grdst,
				  char *buf, size_t buflen, struct group **grdstp);
static struct group *nwrap_files_getgrgid(struct nwrap_backend *b,
					  gid_t gid);
static int nwrap_files_getgrgid_r(struct nwrap_backend *b,
				  gid_t gid, struct group *grdst,
				  char *buf, size_t buflen, struct group **grdstp);
static void nwrap_files_setgrent(struct nwrap_backend *b);
static struct group *nwrap_files_getgrent(struct nwrap_backend *b);
static int nwrap_files_getgrent_r(struct nwrap_backend *b,
				  struct group *grdst, char *buf,
				  size_t buflen, struct group **grdstp);
static void nwrap_files_endgrent(struct nwrap_backend *b);

/* prototypes for module backend */

static struct passwd *nwrap_module_getpwent(struct nwrap_backend *b);
static int nwrap_module_getpwent_r(struct nwrap_backend *b,
				   struct passwd *pwdst, char *buf,
				   size_t buflen, struct passwd **pwdstp);
static struct passwd *nwrap_module_getpwnam(struct nwrap_backend *b,
					    const char *name);
static int nwrap_module_getpwnam_r(struct nwrap_backend *b,
				   const char *name, struct passwd *pwdst,
				   char *buf, size_t buflen, struct passwd **pwdstp);
static struct passwd *nwrap_module_getpwuid(struct nwrap_backend *b,
					    uid_t uid);
static int nwrap_module_getpwuid_r(struct nwrap_backend *b,
				   uid_t uid, struct passwd *pwdst,
				   char *buf, size_t buflen, struct passwd **pwdstp);
static void nwrap_module_setpwent(struct nwrap_backend *b);
static void nwrap_module_endpwent(struct nwrap_backend *b);
static struct group *nwrap_module_getgrent(struct nwrap_backend *b);
static int nwrap_module_getgrent_r(struct nwrap_backend *b,
				   struct group *grdst, char *buf,
				   size_t buflen, struct group **grdstp);
static struct group *nwrap_module_getgrnam(struct nwrap_backend *b,
					   const char *name);
static int nwrap_module_getgrnam_r(struct nwrap_backend *b,
				   const char *name, struct group *grdst,
				   char *buf, size_t buflen, struct group **grdstp);
static struct group *nwrap_module_getgrgid(struct nwrap_backend *b,
					   gid_t gid);
static int nwrap_module_getgrgid_r(struct nwrap_backend *b,
				   gid_t gid, struct group *grdst,
				   char *buf, size_t buflen, struct group **grdstp);
static void nwrap_module_setgrent(struct nwrap_backend *b);
static void nwrap_module_endgrent(struct nwrap_backend *b);
static int nwrap_module_initgroups(struct nwrap_backend *b,
				   const char *user, gid_t group);

struct nwrap_ops nwrap_files_ops = {
	.nw_getpwnam	= nwrap_files_getpwnam,
	.nw_getpwnam_r	= nwrap_files_getpwnam_r,
	.nw_getpwuid	= nwrap_files_getpwuid,
	.nw_getpwuid_r	= nwrap_files_getpwuid_r,
	.nw_setpwent	= nwrap_files_setpwent,
	.nw_getpwent	= nwrap_files_getpwent,
	.nw_getpwent_r	= nwrap_files_getpwent_r,
	.nw_endpwent	= nwrap_files_endpwent,
	.nw_initgroups	= nwrap_files_initgroups,
	.nw_getgrnam	= nwrap_files_getgrnam,
	.nw_getgrnam_r	= nwrap_files_getgrnam_r,
	.nw_getgrgid	= nwrap_files_getgrgid,
	.nw_getgrgid_r	= nwrap_files_getgrgid_r,
	.nw_setgrent	= nwrap_files_setgrent,
	.nw_getgrent	= nwrap_files_getgrent,
	.nw_getgrent_r	= nwrap_files_getgrent_r,
	.nw_endgrent	= nwrap_files_endgrent,
};

struct nwrap_ops nwrap_module_ops = {
	.nw_getpwnam	= nwrap_module_getpwnam,
	.nw_getpwnam_r	= nwrap_module_getpwnam_r,
	.nw_getpwuid	= nwrap_module_getpwuid,
	.nw_getpwuid_r	= nwrap_module_getpwuid_r,
	.nw_setpwent	= nwrap_module_setpwent,
	.nw_getpwent	= nwrap_module_getpwent,
	.nw_getpwent_r	= nwrap_module_getpwent_r,
	.nw_endpwent	= nwrap_module_endpwent,
	.nw_initgroups	= nwrap_module_initgroups,
	.nw_getgrnam	= nwrap_module_getgrnam,
	.nw_getgrnam_r	= nwrap_module_getgrnam_r,
	.nw_getgrgid	= nwrap_module_getgrgid,
	.nw_getgrgid_r	= nwrap_module_getgrgid_r,
	.nw_setgrent	= nwrap_module_setgrent,
	.nw_getgrent	= nwrap_module_getgrent,
	.nw_getgrent_r	= nwrap_module_getgrent_r,
	.nw_endgrent	= nwrap_module_endgrent,
};

struct nwrap_main {
	const char *nwrap_switch;
	int num_backends;
	struct nwrap_backend *backends;
};

struct nwrap_main *nwrap_main_global;
struct nwrap_main __nwrap_main_global;

struct nwrap_cache {
	const char *path;
	int fd;
	struct stat st;
	uint8_t *buf;
	void *private_data;
	bool (*parse_line)(struct nwrap_cache *, char *line);
	void (*unload)(struct nwrap_cache *);
};

struct nwrap_pw {
	struct nwrap_cache *cache;

	struct passwd *list;
	int num;
	int idx;
};

struct nwrap_cache __nwrap_cache_pw;
struct nwrap_pw nwrap_pw_global;

static bool nwrap_pw_parse_line(struct nwrap_cache *nwrap, char *line);
static void nwrap_pw_unload(struct nwrap_cache *nwrap);

struct nwrap_gr {
	struct nwrap_cache *cache;

	struct group *list;
	int num;
	int idx;
};

struct nwrap_cache __nwrap_cache_gr;
struct nwrap_gr nwrap_gr_global;

static bool nwrap_gr_parse_line(struct nwrap_cache *nwrap, char *line);
static void nwrap_gr_unload(struct nwrap_cache *nwrap);

static void *nwrap_load_module_fn(struct nwrap_backend *b,
				  const char *fn_name)
{
	void *res;
	char *s;

	if (!b->so_handle) {
		NWRAP_ERROR(("%s: no handle\n",
			     __location__));
		return NULL;
	}

	if (asprintf(&s, "_nss_%s_%s", b->name, fn_name) == -1) {
		NWRAP_ERROR(("%s: out of memory\n",
			     __location__));
		return NULL;
	}

	res = dlsym(b->so_handle, s);
	if (!res) {
		NWRAP_ERROR(("%s: cannot find function %s in %s\n",
			     __location__, s, b->so_path));
	}
	free(s);
	s = NULL;
	return res;
}

static struct nwrap_module_nss_fns *nwrap_load_module_fns(struct nwrap_backend *b)
{
	struct nwrap_module_nss_fns *fns;

	if (!b->so_handle) {
		return NULL;
	}

	fns = (struct nwrap_module_nss_fns *)malloc(sizeof(struct nwrap_module_nss_fns));
	if (!fns) {
		return NULL;
	}

	fns->_nss_getpwnam_r	= (NSS_STATUS (*)(const char *, struct passwd *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getpwnam_r");
	fns->_nss_getpwuid_r	= (NSS_STATUS (*)(uid_t, struct passwd *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getpwuid_r");
	fns->_nss_setpwent	= (NSS_STATUS(*)(void))
				  nwrap_load_module_fn(b, "setpwent");
	fns->_nss_getpwent_r	= (NSS_STATUS (*)(struct passwd *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getpwent_r");
	fns->_nss_endpwent	= (NSS_STATUS(*)(void))
				  nwrap_load_module_fn(b, "endpwent");
	fns->_nss_initgroups	= (NSS_STATUS (*)(const char *, gid_t, long int *, long int *, gid_t **, long int, int *))
				  nwrap_load_module_fn(b, "initgroups_dyn");
	fns->_nss_getgrnam_r	= (NSS_STATUS (*)(const char *, struct group *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getgrnam_r");
	fns->_nss_getgrgid_r	= (NSS_STATUS (*)(gid_t, struct group *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getgrgid_r");
	fns->_nss_setgrent	= (NSS_STATUS(*)(void))
				  nwrap_load_module_fn(b, "setgrent");
	fns->_nss_getgrent_r	= (NSS_STATUS (*)(struct group *, char *, size_t, int *))
				  nwrap_load_module_fn(b, "getgrent_r");
	fns->_nss_endgrent	= (NSS_STATUS(*)(void))
				  nwrap_load_module_fn(b, "endgrent");

	return fns;
}

static void *nwrap_load_module(const char *so_path)
{
	void *h;

	if (!so_path || !strlen(so_path)) {
		return NULL;
	}

	h = dlopen(so_path, RTLD_LAZY);
	if (!h) {
		NWRAP_ERROR(("%s: cannot open shared library %s\n",
			     __location__, so_path));
		return NULL;
	}

	return h;
}

static bool nwrap_module_init(const char *name,
			      struct nwrap_ops *ops,
			      const char *so_path,
			      int *num_backends,
			      struct nwrap_backend **backends)
{
	struct nwrap_backend *b;

	*backends = (struct nwrap_backend *)realloc(*backends,
		sizeof(struct nwrap_backend) * ((*num_backends) + 1));
	if (!*backends) {
		NWRAP_ERROR(("%s: out of memory\n",
			     __location__));
		return false;
	}

	b = &((*backends)[*num_backends]);

	b->name = name;
	b->ops = ops;
	b->so_path = so_path;

	if (so_path != NULL) {
		b->so_handle = nwrap_load_module(so_path);
		b->fns = nwrap_load_module_fns(b);
		if (b->fns == NULL) {
			return false;
		}
	} else {
		b->so_handle = NULL;
		b->fns = NULL;
	}

	(*num_backends)++;

	return true;
}

static void nwrap_backend_init(struct nwrap_main *r)
{
	const char *winbind_so_path = getenv("NSS_WRAPPER_WINBIND_SO_PATH");

	r->num_backends = 0;
	r->backends = NULL;

	if (!nwrap_module_init("files", &nwrap_files_ops, NULL,
			       &r->num_backends,
			       &r->backends)) {
		NWRAP_ERROR(("%s: failed to initialize 'files' backend\n",
			     __location__));
		return;
	}

	if (winbind_so_path && strlen(winbind_so_path)) {
		if (!nwrap_module_init("winbind", &nwrap_module_ops, winbind_so_path,
				       &r->num_backends,
				       &r->backends)) {
			NWRAP_ERROR(("%s: failed to initialize 'winbind' backend\n",
				     __location__));
			return;
		}
	}
}

static void nwrap_init(void)
{
	static bool initialized;

	if (initialized) return;
	initialized = true;

	nwrap_main_global = &__nwrap_main_global;

	nwrap_backend_init(nwrap_main_global);

	nwrap_pw_global.cache = &__nwrap_cache_pw;

	nwrap_pw_global.cache->path = getenv("NSS_WRAPPER_PASSWD");
	nwrap_pw_global.cache->fd = -1;
	nwrap_pw_global.cache->private_data = &nwrap_pw_global;
	nwrap_pw_global.cache->parse_line = nwrap_pw_parse_line;
	nwrap_pw_global.cache->unload = nwrap_pw_unload;

	nwrap_gr_global.cache = &__nwrap_cache_gr;

	nwrap_gr_global.cache->path = getenv("NSS_WRAPPER_GROUP");
	nwrap_gr_global.cache->fd = -1;
	nwrap_gr_global.cache->private_data = &nwrap_gr_global;
	nwrap_gr_global.cache->parse_line = nwrap_gr_parse_line;
	nwrap_gr_global.cache->unload = nwrap_gr_unload;
}

static bool nwrap_enabled(void)
{
	nwrap_init();

	if (!nwrap_pw_global.cache->path) {
		return false;
	}
	if (nwrap_pw_global.cache->path[0] == '\0') {
		return false;
	}
	if (!nwrap_gr_global.cache->path) {
		return false;
	}
	if (nwrap_gr_global.cache->path[0] == '\0') {
		return false;
	}

	return true;
}

static bool nwrap_parse_file(struct nwrap_cache *nwrap)
{
	int ret;
	uint8_t *buf = NULL;
	char *nline;

	if (nwrap->st.st_size == 0) {
		NWRAP_DEBUG(("%s: size == 0\n",
			     __location__));
		goto done;
	}

	if (nwrap->st.st_size > INT32_MAX) {
		NWRAP_ERROR(("%s: size[%u] larger than INT32_MAX\n",
			     __location__, (unsigned)nwrap->st.st_size));
		goto failed;
	}

	ret = lseek(nwrap->fd, 0, SEEK_SET);
	if (ret != 0) {
		NWRAP_ERROR(("%s: lseek - %d\n",__location__,ret));
		goto failed;
	}

	buf = (uint8_t *)malloc(nwrap->st.st_size + 1);
	if (!buf) {
		NWRAP_ERROR(("%s: malloc failed\n",__location__));
		goto failed;
	}

	ret = read(nwrap->fd, buf, nwrap->st.st_size);
	if (ret != nwrap->st.st_size) {
		NWRAP_ERROR(("%s: read(%u) gave %d\n",
			     __location__, (unsigned)nwrap->st.st_size, ret));
		goto failed;
	}

	buf[nwrap->st.st_size] = '\0';

	nline = (char *)buf;
	while (nline && nline[0]) {
		char *line;
		char *e;
		bool ok;

		line = nline;
		nline = NULL;

		e = strchr(line, '\n');
		if (e) {
			e[0] = '\0';
			e++;
			if (e[0] == '\r') {
				e[0] = '\0';
				e++;
			}
			nline = e;
		}

		NWRAP_VERBOSE(("%s:'%s'\n",__location__, line));

		if (strlen(line) == 0) {
			continue;
		}

		ok = nwrap->parse_line(nwrap, line);
		if (!ok) {
			goto failed;
		}
	}

done:
	nwrap->buf = buf;
	return true;

failed:
	if (buf) free(buf);
	return false;
}

static void nwrap_files_cache_unload(struct nwrap_cache *nwrap)
{
	nwrap->unload(nwrap);

	if (nwrap->buf) free(nwrap->buf);

	nwrap->buf = NULL;
}

static void nwrap_files_cache_reload(struct nwrap_cache *nwrap)
{
	struct stat st;
	int ret;
	bool ok;
	bool retried = false;

reopen:
	if (nwrap->fd < 0) {
		nwrap->fd = open(nwrap->path, O_RDONLY);
		if (nwrap->fd < 0) {
			NWRAP_ERROR(("%s: unable to open '%s' readonly %d:%s\n",
				     __location__,
				     nwrap->path, nwrap->fd,
				     strerror(errno)));
			return;
		}
		NWRAP_VERBOSE(("%s: open '%s'\n", __location__, nwrap->path));
	}

	ret = fstat(nwrap->fd, &st);
	if (ret != 0) {
		NWRAP_ERROR(("%s: fstat(%s) - %d:%s\n",
			     __location__,
			     nwrap->path,
			     ret, strerror(errno)));
		return;
	}

	if (retried == false && st.st_nlink == 0) {
		/* maybe someone has replaced the file... */
		NWRAP_DEBUG(("%s: st_nlink == 0, reopen %s\n",
			     __location__, nwrap->path));
		retried = true;
		memset(&nwrap->st, 0, sizeof(nwrap->st));
		close(nwrap->fd);
		nwrap->fd = -1;
		goto reopen;
	}

	if (st.st_mtime == nwrap->st.st_mtime) {
		NWRAP_VERBOSE(("%s: st_mtime[%u] hasn't changed, skip reload\n",
			       __location__, (unsigned)st.st_mtime));
		return;
	}
	NWRAP_DEBUG(("%s: st_mtime has changed [%u] => [%u], start reload\n",
		     __location__, (unsigned)st.st_mtime,
		     (unsigned)nwrap->st.st_mtime));

	nwrap->st = st;

	nwrap_files_cache_unload(nwrap);

	ok = nwrap_parse_file(nwrap);
	if (!ok) {
		NWRAP_ERROR(("%s: failed to reload %s\n",
			     __location__, nwrap->path));
		nwrap_files_cache_unload(nwrap);
	}
	NWRAP_DEBUG(("%s: reloaded %s\n",
		     __location__, nwrap->path));
}

/*
 * the caller has to call nwrap_unload() on failure
 */
static bool nwrap_pw_parse_line(struct nwrap_cache *nwrap, char *line)
{
	struct nwrap_pw *nwrap_pw;
	char *c;
	char *p;
	char *e;
	struct passwd *pw;
	size_t list_size;

	nwrap_pw = (struct nwrap_pw *)nwrap->private_data;

	list_size = sizeof(*nwrap_pw->list) * (nwrap_pw->num+1);
	pw = (struct passwd *)realloc(nwrap_pw->list, list_size);
	if (!pw) {
		NWRAP_ERROR(("%s:realloc(%u) failed\n",
			     __location__, list_size));
		return false;
	}
	nwrap_pw->list = pw;

	pw = &nwrap_pw->list[nwrap_pw->num];

	c = line;

	/* name */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	pw->pw_name = c;
	c = p;

	NWRAP_VERBOSE(("name[%s]\n", pw->pw_name));

	/* password */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	pw->pw_passwd = c;
	c = p;

	NWRAP_VERBOSE(("password[%s]\n", pw->pw_passwd));

	/* uid */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	e = NULL;
	pw->pw_uid = (uid_t)strtoul(c, &e, 10);
	if (c == e) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e == NULL) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e[0] != '\0') {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	c = p;

	NWRAP_VERBOSE(("uid[%u]\n", pw->pw_uid));

	/* gid */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	e = NULL;
	pw->pw_gid = (gid_t)strtoul(c, &e, 10);
	if (c == e) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e == NULL) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e[0] != '\0') {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	c = p;

	NWRAP_VERBOSE(("gid[%u]\n", pw->pw_gid));

	/* gecos */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	pw->pw_gecos = c;
	c = p;

	NWRAP_VERBOSE(("gecos[%s]\n", pw->pw_gecos));

	/* dir */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:'%s'\n",__location__,c));
		return false;
	}
	*p = '\0';
	p++;
	pw->pw_dir = c;
	c = p;

	NWRAP_VERBOSE(("dir[%s]\n", pw->pw_dir));

	/* shell */
	pw->pw_shell = c;
	NWRAP_VERBOSE(("shell[%s]\n", pw->pw_shell));

	NWRAP_DEBUG(("add user[%s:%s:%u:%u:%s:%s:%s]\n",
		     pw->pw_name, pw->pw_passwd,
		     pw->pw_uid, pw->pw_gid,
		     pw->pw_gecos, pw->pw_dir, pw->pw_shell));

	nwrap_pw->num++;
	return true;
}

static void nwrap_pw_unload(struct nwrap_cache *nwrap)
{
	struct nwrap_pw *nwrap_pw;
	nwrap_pw = (struct nwrap_pw *)nwrap->private_data;

	if (nwrap_pw->list) free(nwrap_pw->list);

	nwrap_pw->list = NULL;
	nwrap_pw->num = 0;
	nwrap_pw->idx = 0;
}

static int nwrap_pw_copy_r(const struct passwd *src, struct passwd *dst,
			   char *buf, size_t buflen, struct passwd **dstp)
{
	char *first;
	char *last;
	off_t ofs;

	first = src->pw_name;

	last = src->pw_shell;
	while (*last) last++;

	ofs = PTR_DIFF(last + 1, first);

	if (ofs > buflen) {
		return ERANGE;
	}

	memcpy(buf, first, ofs);

	ofs = PTR_DIFF(src->pw_name, first);
	dst->pw_name = buf + ofs;
	ofs = PTR_DIFF(src->pw_passwd, first);
	dst->pw_passwd = buf + ofs;
	dst->pw_uid = src->pw_uid;
	dst->pw_gid = src->pw_gid;
	ofs = PTR_DIFF(src->pw_gecos, first);
	dst->pw_gecos = buf + ofs;
	ofs = PTR_DIFF(src->pw_dir, first);
	dst->pw_dir = buf + ofs;
	ofs = PTR_DIFF(src->pw_shell, first);
	dst->pw_shell = buf + ofs;

	if (dstp) {
		*dstp = dst;
	}

	return 0;
}

/*
 * the caller has to call nwrap_unload() on failure
 */
static bool nwrap_gr_parse_line(struct nwrap_cache *nwrap, char *line)
{
	struct nwrap_gr *nwrap_gr;
	char *c;
	char *p;
	char *e;
	struct group *gr;
	size_t list_size;
	unsigned nummem;

	nwrap_gr = (struct nwrap_gr *)nwrap->private_data;

	list_size = sizeof(*nwrap_gr->list) * (nwrap_gr->num+1);
	gr = (struct group *)realloc(nwrap_gr->list, list_size);
	if (!gr) {
		NWRAP_ERROR(("%s:realloc failed\n",__location__));
		return false;
	}
	nwrap_gr->list = gr;

	gr = &nwrap_gr->list[nwrap_gr->num];

	c = line;

	/* name */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	gr->gr_name = c;
	c = p;

	NWRAP_VERBOSE(("name[%s]\n", gr->gr_name));

	/* password */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	gr->gr_passwd = c;
	c = p;

	NWRAP_VERBOSE(("password[%s]\n", gr->gr_passwd));

	/* gid */
	p = strchr(c, ':');
	if (!p) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s'\n",
			     __location__, line, c));
		return false;
	}
	*p = '\0';
	p++;
	e = NULL;
	gr->gr_gid = (gid_t)strtoul(c, &e, 10);
	if (c == e) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e == NULL) {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	if (e[0] != '\0') {
		NWRAP_ERROR(("%s:invalid line[%s]: '%s' - %s\n",
			     __location__, line, c, strerror(errno)));
		return false;
	}
	c = p;

	NWRAP_VERBOSE(("gid[%u]\n", gr->gr_gid));

	/* members */
	gr->gr_mem = (char **)malloc(sizeof(char *));
	if (!gr->gr_mem) {
		NWRAP_ERROR(("%s:calloc failed\n",__location__));
		return false;
	}
	gr->gr_mem[0] = NULL;

	for(nummem=0; p; nummem++) {
		char **m;
		size_t m_size;
		c = p;
		p = strchr(c, ',');
		if (p) {
			*p = '\0';
			p++;
		}

		if (strlen(c) == 0) {
			break;
		}

		m_size = sizeof(char *) * (nummem+2);
		m = (char **)realloc(gr->gr_mem, m_size);
		if (!m) {
			NWRAP_ERROR(("%s:realloc(%u) failed\n",
				      __location__, m_size));
			return false;
		}
		gr->gr_mem = m;
		gr->gr_mem[nummem] = c;
		gr->gr_mem[nummem+1] = NULL;

		NWRAP_VERBOSE(("member[%u]: '%s'\n", nummem, gr->gr_mem[nummem]));
	}

	NWRAP_DEBUG(("add group[%s:%s:%u:] with %u members\n",
		     gr->gr_name, gr->gr_passwd, gr->gr_gid, nummem));

	nwrap_gr->num++;
	return true;
}

static void nwrap_gr_unload(struct nwrap_cache *nwrap)
{
	int i;
	struct nwrap_gr *nwrap_gr;
	nwrap_gr = (struct nwrap_gr *)nwrap->private_data;

	if (nwrap_gr->list) {
		for (i=0; i < nwrap_gr->num; i++) {
			if (nwrap_gr->list[i].gr_mem) {
				free(nwrap_gr->list[i].gr_mem);
			}
		}
		free(nwrap_gr->list);
	}

	nwrap_gr->list = NULL;
	nwrap_gr->num = 0;
	nwrap_gr->idx = 0;
}

static int nwrap_gr_copy_r(const struct group *src, struct group *dst,
			   char *buf, size_t buflen, struct group **dstp)
{
	char *first;
	char **lastm;
	char *last = NULL;
	off_t ofsb;
	off_t ofsm;
	off_t ofs;
	unsigned i;

	first = src->gr_name;

	lastm = src->gr_mem;
	while (*lastm) {
		last = *lastm;
		lastm++;
	}

	if (last == NULL) {
		last = src->gr_passwd;
	}
	while (*last) last++;

	ofsb = PTR_DIFF(last + 1, first);
	ofsm = PTR_DIFF(lastm + 1, src->gr_mem);

	if ((ofsb + ofsm) > buflen) {
		return ERANGE;
	}

	memcpy(buf, first, ofsb);
	memcpy(buf + ofsb, src->gr_mem, ofsm);

	ofs = PTR_DIFF(src->gr_name, first);
	dst->gr_name = buf + ofs;
	ofs = PTR_DIFF(src->gr_passwd, first);
	dst->gr_passwd = buf + ofs;
	dst->gr_gid = src->gr_gid;

	dst->gr_mem = (char **)(buf + ofsb);
	for (i=0; src->gr_mem[i]; i++) {
		ofs = PTR_DIFF(src->gr_mem[i], first);
		dst->gr_mem[i] = buf + ofs;
	}

	if (dstp) {
		*dstp = dst;
	}

	return 0;
}

/* user functions */
static struct passwd *nwrap_files_getpwnam(struct nwrap_backend *b,
					   const char *name)
{
	int i;

	nwrap_files_cache_reload(nwrap_pw_global.cache);

	for (i=0; i<nwrap_pw_global.num; i++) {
		if (strcmp(nwrap_pw_global.list[i].pw_name, name) == 0) {
			NWRAP_DEBUG(("%s: user[%s] found\n",
				     __location__, name));
			return &nwrap_pw_global.list[i];
		}
		NWRAP_VERBOSE(("%s: user[%s] does not match [%s]\n",
			       __location__, name,
			       nwrap_pw_global.list[i].pw_name));
	}

	NWRAP_DEBUG(("%s: user[%s] not found\n", __location__, name));

	errno = ENOENT;
	return NULL;
}

static int nwrap_files_getpwnam_r(struct nwrap_backend *b,
				  const char *name, struct passwd *pwdst,
				  char *buf, size_t buflen, struct passwd **pwdstp)
{
	struct passwd *pw;

	pw = nwrap_files_getpwnam(b, name);
	if (!pw) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_pw_copy_r(pw, pwdst, buf, buflen, pwdstp);
}

static struct passwd *nwrap_files_getpwuid(struct nwrap_backend *b,
					   uid_t uid)
{
	int i;

	nwrap_files_cache_reload(nwrap_pw_global.cache);

	for (i=0; i<nwrap_pw_global.num; i++) {
		if (nwrap_pw_global.list[i].pw_uid == uid) {
			NWRAP_DEBUG(("%s: uid[%u] found\n",
				     __location__, uid));
			return &nwrap_pw_global.list[i];
		}
		NWRAP_VERBOSE(("%s: uid[%u] does not match [%u]\n",
			       __location__, uid,
			       nwrap_pw_global.list[i].pw_uid));
	}

	NWRAP_DEBUG(("%s: uid[%u] not found\n", __location__, uid));

	errno = ENOENT;
	return NULL;
}

static int nwrap_files_getpwuid_r(struct nwrap_backend *b,
				  uid_t uid, struct passwd *pwdst,
				  char *buf, size_t buflen, struct passwd **pwdstp)
{
	struct passwd *pw;

	pw = nwrap_files_getpwuid(b, uid);
	if (!pw) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_pw_copy_r(pw, pwdst, buf, buflen, pwdstp);
}

/* user enum functions */
static void nwrap_files_setpwent(struct nwrap_backend *b)
{
	nwrap_pw_global.idx = 0;
}

static struct passwd *nwrap_files_getpwent(struct nwrap_backend *b)
{
	struct passwd *pw;

	if (nwrap_pw_global.idx == 0) {
		nwrap_files_cache_reload(nwrap_pw_global.cache);
	}

	if (nwrap_pw_global.idx >= nwrap_pw_global.num) {
		errno = ENOENT;
		return NULL;
	}

	pw = &nwrap_pw_global.list[nwrap_pw_global.idx++];

	NWRAP_VERBOSE(("%s: return user[%s] uid[%u]\n",
		       __location__, pw->pw_name, pw->pw_uid));

	return pw;
}

static int nwrap_files_getpwent_r(struct nwrap_backend *b,
				  struct passwd *pwdst, char *buf,
				  size_t buflen, struct passwd **pwdstp)
{
	struct passwd *pw;

	pw = nwrap_files_getpwent(b);
	if (!pw) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_pw_copy_r(pw, pwdst, buf, buflen, pwdstp);
}

static void nwrap_files_endpwent(struct nwrap_backend *b)
{
	nwrap_pw_global.idx = 0;
}

/* misc functions */
static int nwrap_files_initgroups(struct nwrap_backend *b,
				  const char *user, gid_t group)
{
	/* TODO: maybe we should also fake this... */
	return EPERM;
}

/* group functions */
static struct group *nwrap_files_getgrnam(struct nwrap_backend *b,
					  const char *name)
{
	int i;

	nwrap_files_cache_reload(nwrap_gr_global.cache);

	for (i=0; i<nwrap_gr_global.num; i++) {
		if (strcmp(nwrap_gr_global.list[i].gr_name, name) == 0) {
			NWRAP_DEBUG(("%s: group[%s] found\n",
				     __location__, name));
			return &nwrap_gr_global.list[i];
		}
		NWRAP_VERBOSE(("%s: group[%s] does not match [%s]\n",
			       __location__, name,
			       nwrap_gr_global.list[i].gr_name));
	}

	NWRAP_DEBUG(("%s: group[%s] not found\n", __location__, name));

	errno = ENOENT;
	return NULL;
}

static int nwrap_files_getgrnam_r(struct nwrap_backend *b,
				  const char *name, struct group *grdst,
				  char *buf, size_t buflen, struct group **grdstp)
{
	struct group *gr;

	gr = nwrap_files_getgrnam(b, name);
	if (!gr) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_gr_copy_r(gr, grdst, buf, buflen, grdstp);
}

static struct group *nwrap_files_getgrgid(struct nwrap_backend *b,
					  gid_t gid)
{
	int i;

	nwrap_files_cache_reload(nwrap_gr_global.cache);

	for (i=0; i<nwrap_gr_global.num; i++) {
		if (nwrap_gr_global.list[i].gr_gid == gid) {
			NWRAP_DEBUG(("%s: gid[%u] found\n",
				     __location__, gid));
			return &nwrap_gr_global.list[i];
		}
		NWRAP_VERBOSE(("%s: gid[%u] does not match [%u]\n",
			       __location__, gid,
			       nwrap_gr_global.list[i].gr_gid));
	}

	NWRAP_DEBUG(("%s: gid[%u] not found\n", __location__, gid));

	errno = ENOENT;
	return NULL;
}

static int nwrap_files_getgrgid_r(struct nwrap_backend *b,
				  gid_t gid, struct group *grdst,
				  char *buf, size_t buflen, struct group **grdstp)
{
	struct group *gr;

	gr = nwrap_files_getgrgid(b, gid);
	if (!gr) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_gr_copy_r(gr, grdst, buf, buflen, grdstp);
}

/* group enum functions */
static void nwrap_files_setgrent(struct nwrap_backend *b)
{
	nwrap_gr_global.idx = 0;
}

static struct group *nwrap_files_getgrent(struct nwrap_backend *b)
{
	struct group *gr;

	if (nwrap_gr_global.idx == 0) {
		nwrap_files_cache_reload(nwrap_gr_global.cache);
	}

	if (nwrap_gr_global.idx >= nwrap_gr_global.num) {
		errno = ENOENT;
		return NULL;
	}

	gr = &nwrap_gr_global.list[nwrap_gr_global.idx++];

	NWRAP_VERBOSE(("%s: return group[%s] gid[%u]\n",
		       __location__, gr->gr_name, gr->gr_gid));

	return gr;
}

static int nwrap_files_getgrent_r(struct nwrap_backend *b,
				  struct group *grdst, char *buf,
				  size_t buflen, struct group **grdstp)
{
	struct group *gr;

	gr = nwrap_files_getgrent(b);
	if (!gr) {
		if (errno == 0) {
			return ENOENT;
		}
		return errno;
	}

	return nwrap_gr_copy_r(gr, grdst, buf, buflen, grdstp);
}

static void nwrap_files_endgrent(struct nwrap_backend *b)
{
	nwrap_gr_global.idx = 0;
}

/*
 * module backend
 */

#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)
#endif

static struct passwd *nwrap_module_getpwnam(struct nwrap_backend *b,
					    const char *name)
{
	static struct passwd pwd;
	static char buf[1000];
	NSS_STATUS status;

	if (!b->fns->_nss_getpwnam_r) {
		return NULL;
	}

	status = b->fns->_nss_getpwnam_r(name, &pwd, buf, sizeof(buf), &errno);
	if (status == NSS_STATUS_NOTFOUND) {
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		return NULL;
	}
	return &pwd;
}

static int nwrap_module_getpwnam_r(struct nwrap_backend *b,
				   const char *name, struct passwd *pwdst,
				   char *buf, size_t buflen, struct passwd **pwdstp)
{
	int ret;

	if (!b->fns->_nss_getpwnam_r) {
		return NSS_STATUS_NOTFOUND;
	}

	ret = b->fns->_nss_getpwnam_r(name, pwdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static struct passwd *nwrap_module_getpwuid(struct nwrap_backend *b,
					    uid_t uid)
{
	static struct passwd pwd;
	static char buf[1000];
	NSS_STATUS status;

	if (!b->fns->_nss_getpwuid_r) {
		return NULL;
	}

	status = b->fns->_nss_getpwuid_r(uid, &pwd, buf, sizeof(buf), &errno);
	if (status == NSS_STATUS_NOTFOUND) {
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		return NULL;
	}
	return &pwd;
}

static int nwrap_module_getpwuid_r(struct nwrap_backend *b,
				   uid_t uid, struct passwd *pwdst,
				   char *buf, size_t buflen, struct passwd **pwdstp)
{
	int ret;

	if (!b->fns->_nss_getpwuid_r) {
		return ENOENT;
	}

	ret = b->fns->_nss_getpwuid_r(uid, pwdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static void nwrap_module_setpwent(struct nwrap_backend *b)
{
	if (!b->fns->_nss_setpwent) {
		return;
	}

	b->fns->_nss_setpwent();
}

static struct passwd *nwrap_module_getpwent(struct nwrap_backend *b)
{
	static struct passwd pwd;
	static char buf[1000];
	NSS_STATUS status;

	if (!b->fns->_nss_getpwent_r) {
		return NULL;
	}

	status = b->fns->_nss_getpwent_r(&pwd, buf, sizeof(buf), &errno);
	if (status == NSS_STATUS_NOTFOUND) {
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		return NULL;
	}
	return &pwd;
}

static int nwrap_module_getpwent_r(struct nwrap_backend *b,
				   struct passwd *pwdst, char *buf,
				   size_t buflen, struct passwd **pwdstp)
{
	int ret;

	if (!b->fns->_nss_getpwent_r) {
		return ENOENT;
	}

	ret = b->fns->_nss_getpwent_r(pwdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static void nwrap_module_endpwent(struct nwrap_backend *b)
{
	if (!b->fns->_nss_endpwent) {
		return;
	}

	b->fns->_nss_endpwent();
}

static int nwrap_module_initgroups(struct nwrap_backend *b,
				   const char *user, gid_t group)
{
	gid_t *groups;
	long int start;
	long int size;

	if (!b->fns->_nss_initgroups) {
		return NSS_STATUS_UNAVAIL;
	}

	return b->fns->_nss_initgroups(user, group, &start, &size, &groups, 0, &errno);
}

static struct group *nwrap_module_getgrnam(struct nwrap_backend *b,
					   const char *name)
{
	static struct group grp;
	static char *buf;
	static int buflen = 1000;
	NSS_STATUS status;

	if (!b->fns->_nss_getgrnam_r) {
		return NULL;
	}

	if (!buf) {
		buf = (char *)malloc(buflen);
	}
again:
	status = b->fns->_nss_getgrnam_r(name, &grp, buf, buflen, &errno);
	if (status == NSS_STATUS_TRYAGAIN) {
		buflen *= 2;
		buf = (char *)realloc(buf, buflen);
		if (!buf) {
			return NULL;
		}
		goto again;
	}
	if (status == NSS_STATUS_NOTFOUND) {
		SAFE_FREE(buf);
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		SAFE_FREE(buf);
		return NULL;
	}
	return &grp;
}

static int nwrap_module_getgrnam_r(struct nwrap_backend *b,
				   const char *name, struct group *grdst,
				   char *buf, size_t buflen, struct group **grdstp)
{
	int ret;

	if (!b->fns->_nss_getgrnam_r) {
		return ENOENT;
	}

	ret = b->fns->_nss_getgrnam_r(name, grdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static struct group *nwrap_module_getgrgid(struct nwrap_backend *b,
					   gid_t gid)
{
	static struct group grp;
	static char *buf;
	static int buflen = 1000;
	NSS_STATUS status;

	if (!b->fns->_nss_getgrgid_r) {
		return NULL;
	}

	if (!buf) {
		buf = (char *)malloc(buflen);
	}

again:
	status = b->fns->_nss_getgrgid_r(gid, &grp, buf, buflen, &errno);
	if (status == NSS_STATUS_TRYAGAIN) {
		buflen *= 2;
		buf = (char *)realloc(buf, buflen);
		if (!buf) {
			return NULL;
		}
		goto again;
	}
	if (status == NSS_STATUS_NOTFOUND) {
		SAFE_FREE(buf);
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		SAFE_FREE(buf);
		return NULL;
	}
	return &grp;
}

static int nwrap_module_getgrgid_r(struct nwrap_backend *b,
				   gid_t gid, struct group *grdst,
				   char *buf, size_t buflen, struct group **grdstp)
{
	int ret;

	if (!b->fns->_nss_getgrgid_r) {
		return ENOENT;
	}

	ret = b->fns->_nss_getgrgid_r(gid, grdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static void nwrap_module_setgrent(struct nwrap_backend *b)
{
	if (!b->fns->_nss_setgrent) {
		return;
	}

	b->fns->_nss_setgrent();
}

static struct group *nwrap_module_getgrent(struct nwrap_backend *b)
{
	static struct group grp;
	static char *buf;
	static int buflen = 1024;
	NSS_STATUS status;

	if (!b->fns->_nss_getgrent_r) {
		return NULL;
	}

	if (!buf) {
		buf = (char *)malloc(buflen);
	}

again:
	status = b->fns->_nss_getgrent_r(&grp, buf, buflen, &errno);
	if (status == NSS_STATUS_TRYAGAIN) {
		buflen *= 2;
		buf = (char *)realloc(buf, buflen);
		if (!buf) {
			return NULL;
		}
		goto again;
	}
	if (status == NSS_STATUS_NOTFOUND) {
		SAFE_FREE(buf);
		return NULL;
	}
	if (status != NSS_STATUS_SUCCESS) {
		SAFE_FREE(buf);
		return NULL;
	}
	return &grp;
}

static int nwrap_module_getgrent_r(struct nwrap_backend *b,
				   struct group *grdst, char *buf,
				   size_t buflen, struct group **grdstp)
{
	int ret;

	if (!b->fns->_nss_getgrent_r) {
		return ENOENT;
	}

	ret = b->fns->_nss_getgrent_r(grdst, buf, buflen, &errno);
	switch (ret) {
	case NSS_STATUS_SUCCESS:
		return 0;
	case NSS_STATUS_NOTFOUND:
		if (errno != 0) {
			return errno;
		}
		return ENOENT;
	case NSS_STATUS_TRYAGAIN:
		if (errno != 0) {
			return errno;
		}
		return ERANGE;
	default:
		if (errno != 0) {
			return errno;
		}
		return ret;
	}
}

static void nwrap_module_endgrent(struct nwrap_backend *b)
{
	if (!b->fns->_nss_endgrent) {
		return;
	}

	b->fns->_nss_endgrent();
}

/*
 * PUBLIC interface
 */

_PUBLIC_ struct passwd *nwrap_getpwnam(const char *name)
{
	int i;
	struct passwd *pwd;

	if (!nwrap_enabled()) {
		return real_getpwnam(name);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		pwd = b->ops->nw_getpwnam(b, name);
		if (pwd) {
			return pwd;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getpwnam_r(const char *name, struct passwd *pwdst,
			      char *buf, size_t buflen, struct passwd **pwdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
		return real_getpwnam_r(name, pwdst, buf, buflen, pwdstp);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getpwnam_r(b, name, pwdst, buf, buflen, pwdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ struct passwd *nwrap_getpwuid(uid_t uid)
{
	int i;
	struct passwd *pwd;

	if (!nwrap_enabled()) {
		return real_getpwuid(uid);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		pwd = b->ops->nw_getpwuid(b, uid);
		if (pwd) {
			return pwd;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getpwuid_r(uid_t uid, struct passwd *pwdst,
			      char *buf, size_t buflen, struct passwd **pwdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
		return real_getpwuid_r(uid, pwdst, buf, buflen, pwdstp);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getpwuid_r(b, uid, pwdst, buf, buflen, pwdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ void nwrap_setpwent(void)
{
	int i;

	if (!nwrap_enabled()) {
		real_setpwent();
		return;
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		b->ops->nw_setpwent(b);
	}
}

_PUBLIC_ struct passwd *nwrap_getpwent(void)
{
	int i;
	struct passwd *pwd;

	if (!nwrap_enabled()) {
		return real_getpwent();
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		pwd = b->ops->nw_getpwent(b);
		if (pwd) {
			return pwd;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getpwent_r(struct passwd *pwdst, char *buf,
			      size_t buflen, struct passwd **pwdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
#ifdef SOLARIS_GETPWENT_R
		struct passwd *pw;
		pw = real_getpwent_r(pwdst, buf, buflen);
		if (!pw) {
			if (errno == 0) {
				return ENOENT;
			}
			return errno;
		}
		if (pwdstp) {
			*pwdstp = pw;
		}
		return 0;
#else
		return real_getpwent_r(pwdst, buf, buflen, pwdstp);
#endif
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getpwent_r(b, pwdst, buf, buflen, pwdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ void nwrap_endpwent(void)
{
	int i;

	if (!nwrap_enabled()) {
		real_endpwent();
		return;
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		b->ops->nw_endpwent(b);
	}
}

_PUBLIC_ int nwrap_initgroups(const char *user, gid_t group)
{
	int i;

	if (!nwrap_enabled()) {
		return real_initgroups(user, group);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		return b->ops->nw_initgroups(b, user, group);
	}

	errno = ENOENT;
	return -1;
}

_PUBLIC_ struct group *nwrap_getgrnam(const char *name)
{
	int i;
	struct group *grp;

	if (!nwrap_enabled()) {
		return real_getgrnam(name);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		grp = b->ops->nw_getgrnam(b, name);
		if (grp) {
			return grp;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getgrnam_r(const char *name, struct group *grdst,
			      char *buf, size_t buflen, struct group **grdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
		return real_getgrnam_r(name, grdst, buf, buflen, grdstp);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getgrnam_r(b, name, grdst, buf, buflen, grdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ struct group *nwrap_getgrgid(gid_t gid)
{
	int i;
	struct group *grp;

	if (!nwrap_enabled()) {
		return real_getgrgid(gid);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		grp = b->ops->nw_getgrgid(b, gid);
		if (grp) {
			return grp;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getgrgid_r(gid_t gid, struct group *grdst,
			      char *buf, size_t buflen, struct group **grdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
		return real_getgrgid_r(gid, grdst, buf, buflen, grdstp);
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getgrgid_r(b, gid, grdst, buf, buflen, grdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ void nwrap_setgrent(void)
{
	int i;

	if (!nwrap_enabled()) {
		real_setgrent();
		return;
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		b->ops->nw_setgrent(b);
	}
}

_PUBLIC_ struct group *nwrap_getgrent(void)
{
	int i;
	struct group *grp;

	if (!nwrap_enabled()) {
		return real_getgrent();
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		grp = b->ops->nw_getgrent(b);
		if (grp) {
			return grp;
		}
	}

	return NULL;
}

_PUBLIC_ int nwrap_getgrent_r(struct group *grdst, char *buf,
			      size_t buflen, struct group **grdstp)
{
	int i,ret;

	if (!nwrap_enabled()) {
#ifdef SOLARIS_GETGRENT_R
		struct group *gr;
		gr = real_getgrent_r(grdst, buf, buflen);
		if (!gr) {
			if (errno == 0) {
				return ENOENT;
			}
			return errno;
		}
		if (grdstp) {
			*grdstp = gr;
		}
		return 0;
#else
		return real_getgrent_r(grdst, buf, buflen, grdstp);
#endif
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		ret = b->ops->nw_getgrent_r(b, grdst, buf, buflen, grdstp);
		if (ret == ENOENT) {
			continue;
		}
		return ret;
	}

	return ENOENT;
}

_PUBLIC_ void nwrap_endgrent(void)
{
	int i;

	if (!nwrap_enabled()) {
		real_endgrent();
		return;
	}

	for (i=0; i < nwrap_main_global->num_backends; i++) {
		struct nwrap_backend *b = &nwrap_main_global->backends[i];
		b->ops->nw_endgrent(b);
	}
}

_PUBLIC_ int nwrap_getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups)
{
	struct group *grp;
	gid_t *groups_tmp;
	int count = 1;
	const char *name_of_group = "";

	if (!nwrap_enabled()) {
		return real_getgrouplist(user, group, groups, ngroups);
	}

	NWRAP_DEBUG(("%s: getgrouplist called for %s\n", __location__, user));

	groups_tmp = (gid_t *)malloc(count * sizeof(gid_t));
	if (!groups_tmp) {
		NWRAP_ERROR(("%s:calloc failed\n",__location__));
		errno = ENOMEM;
		return -1;
	}

	memcpy(groups_tmp, &group, sizeof(gid_t));

	grp = nwrap_getgrgid(group);
	if (grp) {
		name_of_group = grp->gr_name;
	}

	nwrap_setgrent();
	while ((grp = nwrap_getgrent()) != NULL) {
		int i = 0;

		NWRAP_VERBOSE(("%s: inspecting %s for group membership\n",
			       __location__, grp->gr_name));

		for (i=0; grp->gr_mem && grp->gr_mem[i] != NULL; i++) {

			if ((strcmp(user, grp->gr_mem[i]) == 0) &&
			    (strcmp(name_of_group, grp->gr_name) != 0)) {

				NWRAP_DEBUG(("%s: %s is member of %s\n",
					__location__, user, grp->gr_name));

				groups_tmp = (gid_t *)realloc(groups_tmp, (count + 1) * sizeof(gid_t));
				if (!groups_tmp) {
					NWRAP_ERROR(("%s:calloc failed\n",__location__));
					errno = ENOMEM;
					return -1;
				}

				memcpy(&groups_tmp[count], &grp->gr_gid, sizeof(gid_t));
				count++;
			}
		}
	}

	nwrap_endgrent();

	NWRAP_VERBOSE(("%s: %s is member of %d groups: %d\n",
		       __location__, user, *ngroups));

	if (*ngroups < count) {
		*ngroups = count;
		free(groups_tmp);
		return -1;
	}

	*ngroups = count;
	memcpy(groups, groups_tmp, count * sizeof(gid_t));
	free(groups_tmp);

	return count;
}

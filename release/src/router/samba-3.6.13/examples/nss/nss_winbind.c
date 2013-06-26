/* 
   nss sample code for extended winbindd functionality

   Copyright (C) Andrew Tridgell (tridge@samba.org)   

   you are free to use this code in any way you see fit, including
   without restriction, using this code in your own products. You do
   not need to give any attribution.
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <nss.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "nss_winbind.h"

/*
  find a function in the nss library
*/
static void *find_fn(struct nss_state *nss, const char *name)
{
	void *res;
	char *s = NULL;

	asprintf(&s, "_nss_%s_%s", nss->nss_name, name);
	if (!s) {
		errno = ENOMEM;
		return NULL;
	}
	res = dlsym(nss->dl_handle, s);
	free(s);
	if (!res) {
		errno = ENOENT;
		return NULL;
	}
	return res;
}

/*
  establish a link to the nss library
  Return 0 on success and -1 on error
*/
int nss_open(struct nss_state *nss, const char *nss_path)
{
	char *p;
	p = strrchr(nss_path, '_');
	if (!p) {
		errno = EINVAL;
		return -1;
	}

	nss->nss_name = strdup(p+1);
	p = strchr(nss->nss_name, '.');
	if (p) *p = 0;

	nss->dl_handle = dlopen(nss_path, RTLD_LAZY);
	if (!nss->dl_handle) {
		free(nss->nss_name);
		return -1;
	}

	return 0;
}

/*
  close and cleanup a nss state
*/
void nss_close(struct nss_state *nss)
{
	free(nss->nss_name);
	dlclose(nss->dl_handle);
}

/*
  make a getpwnam call. 
  Return 0 on success and -1 on error
*/
int nss_getpwent(struct nss_state *nss, struct passwd *pwd)
{
	enum nss_status (*_nss_getpwent_r)(struct passwd *, char *, 
					   size_t , int *);
	enum nss_status status;
	int nss_errno = 0;

	_nss_getpwent_r = find_fn(nss, "getpwent_r");

	if (!_nss_getpwent_r) {
		return -1;
	}

	status = _nss_getpwent_r(pwd, nss->pwnam_buf, sizeof(nss->pwnam_buf),
				 &nss_errno);
	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}
	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	return 0;
}

/*
  make a setpwent call. 
  Return 0 on success and -1 on error
*/
int nss_setpwent(struct nss_state *nss)
{
	enum nss_status (*_nss_setpwent)(void) = find_fn(nss, "setpwent");
	enum nss_status status;
	if (!_nss_setpwent) {
		return -1;
	}
	status = _nss_setpwent();
	if (status != NSS_STATUS_SUCCESS) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

/*
  make a endpwent call. 
  Return 0 on success and -1 on error
*/
int nss_endpwent(struct nss_state *nss)
{
	enum nss_status (*_nss_endpwent)(void) = find_fn(nss, "endpwent");
	enum nss_status status;
	if (!_nss_endpwent) {
		return -1;
	}
	status = _nss_endpwent();
	if (status != NSS_STATUS_SUCCESS) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}


/*
  convert a name to a SID
  caller frees
  Return 0 on success and -1 on error
*/
int nss_nametosid(struct nss_state *nss, const char *name, char **sid)
{
	enum nss_status (*_nss_nametosid)(const char *, char **, char *,
					  size_t, int *);
	enum nss_status status;
	int nss_errno = 0;
	char buf[200];

	_nss_nametosid = find_fn(nss, "nametosid");

	if (!_nss_nametosid) {
		return -1;
	}

	status = _nss_nametosid(name, sid, buf, sizeof(buf), &nss_errno);
	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}
	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	*sid = strdup(*sid);

	return 0;
}

/*
  convert a SID to a name
  caller frees
  Return 0 on success and -1 on error
*/
int nss_sidtoname(struct nss_state *nss, const char *sid, char **name)
{
	enum nss_status (*_nss_sidtoname)(const char *, char **, char *,
					  size_t, int *);
	enum nss_status status;
	int nss_errno = 0;
	char buf[200];

	_nss_sidtoname = find_fn(nss, "sidtoname");

	if (!_nss_sidtoname) {
		return -1;
	}

	status = _nss_sidtoname(sid, name, buf, sizeof(buf), &nss_errno);
	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}
	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	*name = strdup(*name);

	return 0;
}

/*
  return a list of group SIDs for a user SID
  the returned list is NULL terminated
  Return 0 on success and -1 on error
*/
int nss_getusersids(struct nss_state *nss, const char *user_sid, char ***sids)
{
	enum nss_status (*_nss_getusersids)(const char *, char **, int *,
					    char *, size_t, int *);
	enum nss_status status;
	int nss_errno = 0;
	char *s;
	int i, num_groups = 0;
	unsigned bufsize = 10;
	char *buf;

	_nss_getusersids = find_fn(nss, "getusersids");

	if (!_nss_getusersids) {
		return -1;
	}

again:
	buf = malloc(bufsize);
	if (!buf) {
		errno = ENOMEM;
		return -1;
	}

	status = _nss_getusersids(user_sid, &s, &num_groups, buf, bufsize,
				  &nss_errno);

	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		free(buf);
		return -1;
	}
	
	if (status == NSS_STATUS_TRYAGAIN) {
		bufsize *= 2;
		free(buf);
		goto again;
	}

	if (status != NSS_STATUS_SUCCESS) {
		free(buf);
		errno = nss_errno;
		return -1;
	}

	if (num_groups == 0) {
		free(buf);
		return 0;
	}

	*sids = (char **)malloc(sizeof(char *) * (num_groups+1));
	if (! *sids) {
		errno = ENOMEM;
		free(buf);
		return -1;
	}

	for (i=0;i<num_groups;i++) {
		(*sids)[i] = strdup(s);
		s += strlen(s) + 1;
	}
	(*sids)[i] = NULL;

	free(buf);

	return 0;
}

/*
  convert a sid to a uid
  Return 0 on success and -1 on error
*/
int nss_sidtouid(struct nss_state *nss, const char *sid, uid_t *uid)
{
	enum nss_status (*_nss_sidtouid)(const char*, uid_t *, int*);

	enum nss_status status;
	int nss_errno = 0;

	_nss_sidtouid = find_fn(nss, "sidtouid");

	if (!_nss_sidtouid) {
		return -1;
	}

	status = _nss_sidtouid(sid, uid, &nss_errno);

	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}

	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	return 0;
}

/*
  convert a sid to a gid
  Return 0 on success and -1 on error
*/
int nss_sidtogid(struct nss_state *nss, const char *sid, gid_t *gid)
{
	enum nss_status (*_nss_sidtogid)(const char*, gid_t *, int*);

	enum nss_status status;
	int nss_errno = 0;

	_nss_sidtogid = find_fn(nss, "sidtogid");

	if (!_nss_sidtogid) {
		return -1;
	}

	status = _nss_sidtogid(sid, gid, &nss_errno);

	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}

	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	return 0;
}

/*
  convert a uid to a sid
  caller frees
  Return 0 on success and -1 on error
*/
int nss_uidtosid(struct nss_state *nss, uid_t uid, char **sid)
{
	enum nss_status (*_nss_uidtosid)(uid_t, char **, char *,
					 size_t, int *);
	enum nss_status status;
	int nss_errno = 0;
	char buf[200];

	_nss_uidtosid = find_fn(nss, "uidtosid");

	if (!_nss_uidtosid) {
		return -1;
	}

	status = _nss_uidtosid(uid, sid, buf, sizeof(buf), &nss_errno);
	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}
	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	*sid = strdup(*sid);

	return 0;
}

/*
  convert a gid to a sid
  caller frees
  Return 0 on success and -1 on error
*/
int nss_gidtosid(struct nss_state *nss, gid_t gid, char **sid)
{
	enum nss_status (*_nss_gidtosid)(gid_t, char **, char *,
					 size_t, int *);
	enum nss_status status;
	int nss_errno = 0;
	char buf[200];

	_nss_gidtosid = find_fn(nss, "gidtosid");

	if (!_nss_gidtosid) {
		return -1;
	}

	status = _nss_gidtosid(gid, sid, buf, sizeof(buf), &nss_errno);
	if (status == NSS_STATUS_NOTFOUND) {
		errno = ENOENT;
		return -1;
	}
	if (status != NSS_STATUS_SUCCESS) {
		errno = nss_errno;
		return -1;
	}

	*sid = strdup(*sid);

	return 0;
}


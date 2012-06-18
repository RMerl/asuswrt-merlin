/*
   Unix SMB/CIFS implementation.

   Windows NT Domain nsswitch module

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

#include "winbind_client.h"

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#if HAVE_PTHREAD
static pthread_mutex_t winbind_nss_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Maximum number of users to pass back over the unix domain socket
   per call. This is not a static limit on the total number of users
   or groups returned in total. */

#define MAX_GETPWENT_USERS 250
#define MAX_GETGRENT_USERS 250

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

/* Prototypes from wb_common.c */

/* Allocate some space from the nss static buffer.  The buffer and buflen
   are the pointers passed in by the C library to the _nss_ntdom_*
   functions. */

static char *get_static(char **buffer, size_t *buflen, size_t len)
{
	char *result;

	/* Error check.  We return false if things aren't set up right, or
	   there isn't enough buffer space left. */

	if ((buffer == NULL) || (buflen == NULL) || (*buflen < len)) {
		return NULL;
	}

	/* Return an index into the static buffer */

	result = *buffer;
	*buffer += len;
	*buflen -= len;

	return result;
}

/* I've copied the strtok() replacement function next_token_Xalloc() from
   lib/util_str.c as I really don't want to have to link in any other
   objects if I can possibly avoid it. */

static bool next_token_alloc(const char **ptr,
                                char **pp_buff,
                                const char *sep)
{
	const char *s;
	const char *saved_s;
	char *pbuf;
	bool quoted;
	size_t len=1;

	*pp_buff = NULL;
	if (!ptr) {
		return(false);
	}

	s = *ptr;

	/* default to simple separators */
	if (!sep) {
		sep = " \t\n\r";
	}

	/* find the first non sep char */
	while (*s && strchr(sep,*s)) {
		s++;
	}

	/* nothing left? */
	if (!*s) {
		return false;
	}

	/* When restarting we need to go from here. */
	saved_s = s;

	/* Work out the length needed. */
	for (quoted = false; *s &&
			(quoted || !strchr(sep,*s)); s++) {
		if (*s == '\"') {
			quoted = !quoted;
		} else {
			len++;
		}
	}

	/* We started with len = 1 so we have space for the nul. */
	*pp_buff = (char *)malloc(len);
	if (!*pp_buff) {
		return false;
	}

	/* copy over the token */
	pbuf = *pp_buff;
	s = saved_s;
	for (quoted = false; *s &&
			(quoted || !strchr(sep,*s)); s++) {
		if ( *s == '\"' ) {
			quoted = !quoted;
		} else {
			*pbuf++ = *s;
		}
	}

	*ptr = (*s) ? s+1 : s;
	*pbuf = 0;

	return true;
}

/* Fill a pwent structure from a winbindd_response structure.  We use
   the static data passed to us by libc to put strings and stuff in.
   Return NSS_STATUS_TRYAGAIN if we run out of memory. */

static NSS_STATUS fill_pwent(struct passwd *result,
				  struct winbindd_pw *pw,
				  char **buffer, size_t *buflen)
{
	/* User name */

	if ((result->pw_name =
	     get_static(buffer, buflen, strlen(pw->pw_name) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->pw_name, pw->pw_name);

	/* Password */

	if ((result->pw_passwd =
	     get_static(buffer, buflen, strlen(pw->pw_passwd) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->pw_passwd, pw->pw_passwd);

	/* [ug]id */

	result->pw_uid = pw->pw_uid;
	result->pw_gid = pw->pw_gid;

	/* GECOS */

	if ((result->pw_gecos =
	     get_static(buffer, buflen, strlen(pw->pw_gecos) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->pw_gecos, pw->pw_gecos);

	/* Home directory */

	if ((result->pw_dir =
	     get_static(buffer, buflen, strlen(pw->pw_dir) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->pw_dir, pw->pw_dir);

	/* Logon shell */

	if ((result->pw_shell =
	     get_static(buffer, buflen, strlen(pw->pw_shell) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->pw_shell, pw->pw_shell);

	/* The struct passwd for Solaris has some extra fields which must
	   be initialised or nscd crashes. */

#if HAVE_PASSWD_PW_COMMENT
	result->pw_comment = "";
#endif

#if HAVE_PASSWD_PW_AGE
	result->pw_age = "";
#endif

	return NSS_STATUS_SUCCESS;
}

/* Fill a grent structure from a winbindd_response structure.  We use
   the static data passed to us by libc to put strings and stuff in.
   Return NSS_STATUS_TRYAGAIN if we run out of memory. */

static NSS_STATUS fill_grent(struct group *result, struct winbindd_gr *gr,
		      char *gr_mem, char **buffer, size_t *buflen)
{
	char *name;
	int i;
	char *tst;

	/* Group name */

	if ((result->gr_name =
	     get_static(buffer, buflen, strlen(gr->gr_name) + 1)) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->gr_name, gr->gr_name);

	/* Password */

	if ((result->gr_passwd =
	     get_static(buffer, buflen, strlen(gr->gr_passwd) + 1)) == NULL) {

		/* Out of memory */
		return NSS_STATUS_TRYAGAIN;
	}

	strcpy(result->gr_passwd, gr->gr_passwd);

	/* gid */

	result->gr_gid = gr->gr_gid;

	/* Group membership */

	if ((gr->num_gr_mem < 0) || !gr_mem) {
		gr->num_gr_mem = 0;
	}

	/* this next value is a pointer to a pointer so let's align it */

	/* Calculate number of extra bytes needed to align on pointer size boundry */
	if ((i = (unsigned long)(*buffer) % sizeof(char*)) != 0)
		i = sizeof(char*) - i;

	if ((tst = get_static(buffer, buflen, ((gr->num_gr_mem + 1) *
				 sizeof(char *)+i))) == NULL) {

		/* Out of memory */

		return NSS_STATUS_TRYAGAIN;
	}
	result->gr_mem = (char **)(tst + i);

	if (gr->num_gr_mem == 0) {

		/* Group is empty */

		*(result->gr_mem) = NULL;
		return NSS_STATUS_SUCCESS;
	}

	/* Start looking at extra data */

	i = 0;

	while(next_token_alloc((const char **)&gr_mem, &name, ",")) {
		/* Allocate space for member */
		if (((result->gr_mem)[i] =
		     get_static(buffer, buflen, strlen(name) + 1)) == NULL) {
			free(name);
			/* Out of memory */
			return NSS_STATUS_TRYAGAIN;
		}
		strcpy((result->gr_mem)[i], name);
		free(name);
		i++;
	}

	/* Terminate list */

	(result->gr_mem)[i] = NULL;

	return NSS_STATUS_SUCCESS;
}

/*
 * NSS user functions
 */

static struct winbindd_response getpwent_response;

static int ndx_pw_cache;                 /* Current index into pwd cache */
static int num_pw_cache;                 /* Current size of pwd cache */

/* Rewind "file pointer" to start of ntdom password database */

NSS_STATUS
_nss_winbind_setpwent(void)
{
	NSS_STATUS ret;
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: setpwent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	if (num_pw_cache > 0) {
		ndx_pw_cache = num_pw_cache = 0;
		winbindd_free_response(&getpwent_response);
	}

	ret = winbindd_request_response(WINBINDD_SETPWENT, NULL, NULL);
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: setpwent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif
	return ret;
}

/* Close ntdom password database "file pointer" */

NSS_STATUS
_nss_winbind_endpwent(void)
{
	NSS_STATUS ret;
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: endpwent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	if (num_pw_cache > 0) {
		ndx_pw_cache = num_pw_cache = 0;
		winbindd_free_response(&getpwent_response);
	}

	ret = winbindd_request_response(WINBINDD_ENDPWENT, NULL, NULL);
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: endpwent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* Fetch the next password entry from ntdom password database */

NSS_STATUS
_nss_winbind_getpwent_r(struct passwd *result, char *buffer,
			size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_request request;
	static int called_again;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* Return an entry from the cache if we have one, or if we are
	   called again because we exceeded our static buffer.  */

	if ((ndx_pw_cache < num_pw_cache) || called_again) {
		goto return_result;
	}

	/* Else call winbindd to get a bunch of entries */

	if (num_pw_cache > 0) {
		winbindd_free_response(&getpwent_response);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(getpwent_response);

	request.data.num_entries = MAX_GETPWENT_USERS;

	ret = winbindd_request_response(WINBINDD_GETPWENT, &request,
			       &getpwent_response);

	if (ret == NSS_STATUS_SUCCESS) {
		struct winbindd_pw *pw_cache;

		/* Fill cache */

		ndx_pw_cache = 0;
		num_pw_cache = getpwent_response.data.num_entries;

		/* Return a result */

	return_result:

		pw_cache = (struct winbindd_pw *)
			getpwent_response.extra_data.data;

		/* Check data is valid */

		if (pw_cache == NULL) {
			ret = NSS_STATUS_NOTFOUND;
			goto done;
		}

		ret = fill_pwent(result, &pw_cache[ndx_pw_cache],
				 &buffer, &buflen);

		/* Out of memory - try again */

		if (ret == NSS_STATUS_TRYAGAIN) {
			called_again = true;
			*errnop = errno = ERANGE;
			goto done;
		}

		*errnop = errno = 0;
		called_again = false;
		ndx_pw_cache++;

		/* If we've finished with this lot of results free cache */

		if (ndx_pw_cache == num_pw_cache) {
			ndx_pw_cache = num_pw_cache = 0;
			winbindd_free_response(&getpwent_response);
		}
	}
	done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif
	return ret;
}

/* Return passwd struct from uid */

NSS_STATUS
_nss_winbind_getpwuid_r(uid_t uid, struct passwd *result, char *buffer,
			size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	static struct winbindd_response response;
	struct winbindd_request request;
	static int keep_response;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwuid_r %d\n", getpid(), (unsigned int)uid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* If our static buffer needs to be expanded we are called again */
	if (!keep_response || uid != response.data.pw.pw_uid) {

		/* Call for the first time */

		ZERO_STRUCT(response);
		ZERO_STRUCT(request);

		request.data.uid = uid;

		ret = winbindd_request_response(WINBINDD_GETPWUID, &request, &response);

		if (ret == NSS_STATUS_SUCCESS) {
			ret = fill_pwent(result, &response.data.pw,
					 &buffer, &buflen);

			if (ret == NSS_STATUS_TRYAGAIN) {
				keep_response = true;
				*errnop = errno = ERANGE;
				goto done;
			}
		}

	} else {

		/* We've been called again */

		ret = fill_pwent(result, &response.data.pw, &buffer, &buflen);

		if (ret == NSS_STATUS_TRYAGAIN) {
			*errnop = errno = ERANGE;
			goto done;
		}

		keep_response = false;
		*errnop = errno = 0;
	}

	winbindd_free_response(&response);

	done:

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwuid %d returns %s (%d)\n", getpid(),
		(unsigned int)uid, nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* Return passwd struct from username */
NSS_STATUS
_nss_winbind_getpwnam_r(const char *name, struct passwd *result, char *buffer,
			size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	static struct winbindd_response response;
	struct winbindd_request request;
	static int keep_response;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwnam_r %s\n", getpid(), name);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* If our static buffer needs to be expanded we are called again */

	if (!keep_response || strcmp(name,response.data.pw.pw_name) != 0) {

		/* Call for the first time */

		ZERO_STRUCT(response);
		ZERO_STRUCT(request);

		strncpy(request.data.username, name,
			sizeof(request.data.username) - 1);
		request.data.username
			[sizeof(request.data.username) - 1] = '\0';

		ret = winbindd_request_response(WINBINDD_GETPWNAM, &request, &response);

		if (ret == NSS_STATUS_SUCCESS) {
			ret = fill_pwent(result, &response.data.pw, &buffer,
					 &buflen);

			if (ret == NSS_STATUS_TRYAGAIN) {
				keep_response = true;
				*errnop = errno = ERANGE;
				goto done;
			}
		}

	} else {

		/* We've been called again */

		ret = fill_pwent(result, &response.data.pw, &buffer, &buflen);

		if (ret == NSS_STATUS_TRYAGAIN) {
			keep_response = true;
			*errnop = errno = ERANGE;
			goto done;
		}

		keep_response = false;
		*errnop = errno = 0;
	}

	winbindd_free_response(&response);
	done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getpwnam %s returns %s (%d)\n", getpid(),
		name, nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/*
 * NSS group functions
 */

static struct winbindd_response getgrent_response;

static int ndx_gr_cache;                 /* Current index into grp cache */
static int num_gr_cache;                 /* Current size of grp cache */

/* Rewind "file pointer" to start of ntdom group database */

NSS_STATUS
_nss_winbind_setgrent(void)
{
	NSS_STATUS ret;
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: setgrent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	if (num_gr_cache > 0) {
		ndx_gr_cache = num_gr_cache = 0;
		winbindd_free_response(&getgrent_response);
	}

	ret = winbindd_request_response(WINBINDD_SETGRENT, NULL, NULL);
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: setgrent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* Close "file pointer" for ntdom group database */

NSS_STATUS
_nss_winbind_endgrent(void)
{
	NSS_STATUS ret;
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: endgrent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	if (num_gr_cache > 0) {
		ndx_gr_cache = num_gr_cache = 0;
		winbindd_free_response(&getgrent_response);
	}

	ret = winbindd_request_response(WINBINDD_ENDGRENT, NULL, NULL);
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: endgrent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* Get next entry from ntdom group database */

static NSS_STATUS
winbind_getgrent(enum winbindd_cmd cmd,
		 struct group *result,
		 char *buffer, size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	static struct winbindd_request request;
	static int called_again;


#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrent\n", getpid());
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* Return an entry from the cache if we have one, or if we are
	   called again because we exceeded our static buffer.  */

	if ((ndx_gr_cache < num_gr_cache) || called_again) {
		goto return_result;
	}

	/* Else call winbindd to get a bunch of entries */

	if (num_gr_cache > 0) {
		winbindd_free_response(&getgrent_response);
	}

	ZERO_STRUCT(request);
	ZERO_STRUCT(getgrent_response);

	request.data.num_entries = MAX_GETGRENT_USERS;

	ret = winbindd_request_response(cmd, &request,
			       &getgrent_response);

	if (ret == NSS_STATUS_SUCCESS) {
		struct winbindd_gr *gr_cache;
		int mem_ofs;

		/* Fill cache */

		ndx_gr_cache = 0;
		num_gr_cache = getgrent_response.data.num_entries;

		/* Return a result */

	return_result:

		gr_cache = (struct winbindd_gr *)
			getgrent_response.extra_data.data;

		/* Check data is valid */

		if (gr_cache == NULL) {
			ret = NSS_STATUS_NOTFOUND;
			goto done;
		}

		/* Fill group membership.  The offset into the extra data
		   for the group membership is the reported offset plus the
		   size of all the winbindd_gr records returned. */

		mem_ofs = gr_cache[ndx_gr_cache].gr_mem_ofs +
			num_gr_cache * sizeof(struct winbindd_gr);

		ret = fill_grent(result, &gr_cache[ndx_gr_cache],
				 ((char *)getgrent_response.extra_data.data)+mem_ofs,
				 &buffer, &buflen);

		/* Out of memory - try again */

		if (ret == NSS_STATUS_TRYAGAIN) {
			called_again = true;
			*errnop = errno = ERANGE;
			goto done;
		}

		*errnop = 0;
		called_again = false;
		ndx_gr_cache++;

		/* If we've finished with this lot of results free cache */

		if (ndx_gr_cache == num_gr_cache) {
			ndx_gr_cache = num_gr_cache = 0;
			winbindd_free_response(&getgrent_response);
		}
	}
	done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrent returns %s (%d)\n", getpid(),
		nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}


NSS_STATUS
_nss_winbind_getgrent_r(struct group *result,
			char *buffer, size_t buflen, int *errnop)
{
	return winbind_getgrent(WINBINDD_GETGRENT, result, buffer, buflen, errnop);
}

NSS_STATUS
_nss_winbind_getgrlst_r(struct group *result,
			char *buffer, size_t buflen, int *errnop)
{
	return winbind_getgrent(WINBINDD_GETGRLST, result, buffer, buflen, errnop);
}

/* Return group struct from group name */

NSS_STATUS
_nss_winbind_getgrnam_r(const char *name,
			struct group *result, char *buffer,
			size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	static struct winbindd_response response;
	struct winbindd_request request;
	static int keep_response;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrnam %s\n", getpid(), name);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* If our static buffer needs to be expanded we are called again */
	/* Or if the stored response group name differs from the request. */

	if (!keep_response || strcmp(name,response.data.gr.gr_name) != 0) {

		/* Call for the first time */

		ZERO_STRUCT(request);
		ZERO_STRUCT(response);

		strncpy(request.data.groupname, name,
			sizeof(request.data.groupname));
		request.data.groupname
			[sizeof(request.data.groupname) - 1] = '\0';

		ret = winbindd_request_response(WINBINDD_GETGRNAM, &request, &response);

		if (ret == NSS_STATUS_SUCCESS) {
			ret = fill_grent(result, &response.data.gr,
					 (char *)response.extra_data.data,
					 &buffer, &buflen);

			if (ret == NSS_STATUS_TRYAGAIN) {
				keep_response = true;
				*errnop = errno = ERANGE;
				goto done;
			}
		}

	} else {

		/* We've been called again */

		ret = fill_grent(result, &response.data.gr,
				 (char *)response.extra_data.data, &buffer,
				 &buflen);

		if (ret == NSS_STATUS_TRYAGAIN) {
			keep_response = true;
			*errnop = errno = ERANGE;
			goto done;
		}

		keep_response = false;
		*errnop = 0;
	}

	winbindd_free_response(&response);
	done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrnam %s returns %s (%d)\n", getpid(),
		name, nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* Return group struct from gid */

NSS_STATUS
_nss_winbind_getgrgid_r(gid_t gid,
			struct group *result, char *buffer,
			size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	static struct winbindd_response response;
	struct winbindd_request request;
	static int keep_response;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrgid %d\n", getpid(), gid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	/* If our static buffer needs to be expanded we are called again */
	/* Or if the stored response group name differs from the request. */

	if (!keep_response || gid != response.data.gr.gr_gid) {

		/* Call for the first time */

		ZERO_STRUCT(request);
		ZERO_STRUCT(response);

		request.data.gid = gid;

		ret = winbindd_request_response(WINBINDD_GETGRGID, &request, &response);

		if (ret == NSS_STATUS_SUCCESS) {

			ret = fill_grent(result, &response.data.gr,
					 (char *)response.extra_data.data,
					 &buffer, &buflen);

			if (ret == NSS_STATUS_TRYAGAIN) {
				keep_response = true;
				*errnop = errno = ERANGE;
				goto done;
			}
		}

	} else {

		/* We've been called again */

		ret = fill_grent(result, &response.data.gr,
				 (char *)response.extra_data.data, &buffer,
				 &buflen);

		if (ret == NSS_STATUS_TRYAGAIN) {
			keep_response = true;
			*errnop = errno = ERANGE;
			goto done;
		}

		keep_response = false;
		*errnop = 0;
	}

	winbindd_free_response(&response);
	done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getgrgid %d returns %s (%d)\n", getpid(),
		(unsigned int)gid, nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif
	return ret;
}

/* Initialise supplementary groups */

NSS_STATUS
_nss_winbind_initgroups_dyn(char *user, gid_t group, long int *start,
			    long int *size, gid_t **groups, long int limit,
			    int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_request request;
	struct winbindd_response response;
	int i;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: initgroups %s (%d)\n", getpid(),
		user, group);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.username, user,
		sizeof(request.data.username) - 1);

	ret = winbindd_request_response(WINBINDD_GETGROUPS, &request, &response);

	if (ret == NSS_STATUS_SUCCESS) {
		int num_gids = response.data.num_entries;
		gid_t *gid_list = (gid_t *)response.extra_data.data;

#ifdef DEBUG_NSS
		fprintf(stderr, "[%5d]: initgroups %s: got NSS_STATUS_SUCCESS "
				"and %d gids\n", getpid(),
				user, num_gids);
#endif
		if (gid_list == NULL) {
			ret = NSS_STATUS_NOTFOUND;
			goto done;
		}

		/* Copy group list to client */

		for (i = 0; i < num_gids; i++) {

#ifdef DEBUG_NSS
			fprintf(stderr, "[%5d]: initgroups %s (%d): "
					"processing gid %d \n", getpid(),
					user, group, gid_list[i]);
#endif

			/* Skip primary group */

			if (gid_list[i] == group) {
				continue;
			}

			/* Filled buffer ? If so, resize. */

			if (*start == *size) {
				long int newsize;
				gid_t *newgroups;

				newsize = 2 * (*size);
				if (limit > 0) {
					if (*size == limit) {
						goto done;
					}
					if (newsize > limit) {
						newsize = limit;
					}
				}

				newgroups = (gid_t *)
					realloc((*groups),
						newsize * sizeof(**groups));
				if (!newgroups) {
					*errnop = ENOMEM;
					ret = NSS_STATUS_NOTFOUND;
					goto done;
				}
				*groups = newgroups;
				*size = newsize;
			}

			/* Add to buffer */

			(*groups)[*start] = gid_list[i];
			*start += 1;
		}
	}

	/* Back to your regularly scheduled programming */

 done:
#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: initgroups %s returns %s (%d)\n", getpid(),
		user, nss_err_str(ret), ret);
#endif

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}


/* return a list of group SIDs for a user SID */
NSS_STATUS
_nss_winbind_getusersids(const char *user_sid, char **group_sids,
			 int *num_groups,
			 char *buffer, size_t buf_size, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_request request;
	struct winbindd_response response;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: getusersids %s\n", getpid(), user_sid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.sid, user_sid,sizeof(request.data.sid) - 1);
	request.data.sid[sizeof(request.data.sid) - 1] = '\0';

	ret = winbindd_request_response(WINBINDD_GETUSERSIDS, &request, &response);

	if (ret != NSS_STATUS_SUCCESS) {
		goto done;
	}

	if (buf_size < response.length - sizeof(response)) {
		ret = NSS_STATUS_TRYAGAIN;
		errno = *errnop = ERANGE;
		goto done;
	}

	*num_groups = response.data.num_entries;
	*group_sids = buffer;
	memcpy(buffer, response.extra_data.data, response.length - sizeof(response));
	errno = *errnop = 0;

 done:
	winbindd_free_response(&response);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}


/* map a user or group name to a SID string */
NSS_STATUS
_nss_winbind_nametosid(const char *name, char **sid, char *buffer,
		       size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: nametosid %s\n", getpid(), name);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(response);
	ZERO_STRUCT(request);

	strncpy(request.data.name.name, name,
		sizeof(request.data.name.name) - 1);
	request.data.name.name[sizeof(request.data.name.name) - 1] = '\0';

	ret = winbindd_request_response(WINBINDD_LOOKUPNAME, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	if (buflen < strlen(response.data.sid.sid)+1) {
		ret = NSS_STATUS_TRYAGAIN;
		*errnop = errno = ERANGE;
		goto failed;
	}

	*errnop = errno = 0;
	*sid = buffer;
	strcpy(*sid, response.data.sid.sid);

failed:
	winbindd_free_response(&response);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* map a sid string to a user or group name */
NSS_STATUS
_nss_winbind_sidtoname(const char *sid, char **name, char *buffer,
		       size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;
	static char sep_char;
	unsigned needed;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: sidtoname %s\n", getpid(), sid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(response);
	ZERO_STRUCT(request);

	/* we need to fetch the separator first time through */
	if (!sep_char) {
		ret = winbindd_request_response(WINBINDD_INFO, &request, &response);
		if (ret != NSS_STATUS_SUCCESS) {
			*errnop = errno = EINVAL;
			goto failed;
		}

		sep_char = response.data.info.winbind_separator;
		winbindd_free_response(&response);
	}


	strncpy(request.data.sid, sid,
		sizeof(request.data.sid) - 1);
	request.data.sid[sizeof(request.data.sid) - 1] = '\0';

	ret = winbindd_request_response(WINBINDD_LOOKUPSID, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	needed =
		strlen(response.data.name.dom_name) +
		strlen(response.data.name.name) + 2;

	if (buflen < needed) {
		ret = NSS_STATUS_TRYAGAIN;
		*errnop = errno = ERANGE;
		goto failed;
	}

	snprintf(buffer, needed, "%s%c%s",
		 response.data.name.dom_name,
		 sep_char,
		 response.data.name.name);

	*name = buffer;
	*errnop = errno = 0;

failed:
	winbindd_free_response(&response);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* map a sid to a uid */
NSS_STATUS
_nss_winbind_sidtouid(const char *sid, uid_t *uid, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: sidtouid %s\n", getpid(), sid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.sid, sid, sizeof(request.data.sid) - 1);
	request.data.sid[sizeof(request.data.sid) - 1] = '\0';

	ret = winbindd_request_response(WINBINDD_SID_TO_UID, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	*uid = response.data.uid;

failed:

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* map a sid to a gid */
NSS_STATUS
_nss_winbind_sidtogid(const char *sid, gid_t *gid, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5d]: sidtogid %s\n", getpid(), sid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(request);
	ZERO_STRUCT(response);

	strncpy(request.data.sid, sid, sizeof(request.data.sid) - 1);
	request.data.sid[sizeof(request.data.sid) - 1] = '\0';

	ret = winbindd_request_response(WINBINDD_SID_TO_GID, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	*gid = response.data.gid;

failed:

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* map a uid to a SID string */
NSS_STATUS
_nss_winbind_uidtosid(uid_t uid, char **sid, char *buffer,
		      size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5u]: uidtosid %u\n", (unsigned int)getpid(), (unsigned int)uid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(response);
	ZERO_STRUCT(request);

	request.data.uid = uid;

	ret = winbindd_request_response(WINBINDD_UID_TO_SID, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	if (buflen < strlen(response.data.sid.sid)+1) {
		ret = NSS_STATUS_TRYAGAIN;
		*errnop = errno = ERANGE;
		goto failed;
	}

	*errnop = errno = 0;
	*sid = buffer;
	strcpy(*sid, response.data.sid.sid);

failed:
	winbindd_free_response(&response);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

/* map a gid to a SID string */
NSS_STATUS
_nss_winbind_gidtosid(gid_t gid, char **sid, char *buffer,
		      size_t buflen, int *errnop)
{
	NSS_STATUS ret;
	struct winbindd_response response;
	struct winbindd_request request;

#ifdef DEBUG_NSS
	fprintf(stderr, "[%5u]: gidtosid %u\n", (unsigned int)getpid(), (unsigned int)gid);
#endif

#if HAVE_PTHREAD
	pthread_mutex_lock(&winbind_nss_mutex);
#endif

	ZERO_STRUCT(response);
	ZERO_STRUCT(request);

	request.data.gid = gid;

	ret = winbindd_request_response(WINBINDD_GID_TO_SID, &request, &response);
	if (ret != NSS_STATUS_SUCCESS) {
		*errnop = errno = EINVAL;
		goto failed;
	}

	if (buflen < strlen(response.data.sid.sid)+1) {
		ret = NSS_STATUS_TRYAGAIN;
		*errnop = errno = ERANGE;
		goto failed;
	}

	*errnop = errno = 0;
	*sid = buffer;
	strcpy(*sid, response.data.sid.sid);

failed:
	winbindd_free_response(&response);

#if HAVE_PTHREAD
	pthread_mutex_unlock(&winbind_nss_mutex);
#endif

	return ret;
}

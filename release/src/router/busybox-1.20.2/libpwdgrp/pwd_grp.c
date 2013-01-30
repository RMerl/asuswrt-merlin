/* vi: set sw=4 ts=4: */
/* Copyright (C) 2003     Manuel Novoa III
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Nov 6, 2003  Initial version.
 *
 * NOTE: This implementation is quite strict about requiring all
 *    field seperators.  It also does not allow leading whitespace
 *    except when processing the numeric fields.  glibc is more
 *    lenient.  See the various glibc difference comments below.
 *
 * TODO:
 *    Move to dynamic allocation of (currently statically allocated)
 *      buffers; especially for the group-related functions since
 *      large group member lists will cause error returns.
 */

#include "libbb.h"
#include <assert.h>

/**********************************************************************/
/* Sizes for statically allocated buffers. */

#define PWD_BUFFER_SIZE 256
#define GRP_BUFFER_SIZE 256

/**********************************************************************/
/* Prototypes for internal functions. */

static int bb__pgsreader(
		int FAST_FUNC (*parserfunc)(void *d, char *line),
		void *data,
		char *__restrict line_buff,
		size_t buflen,
		FILE *f);

static int FAST_FUNC bb__parsepwent(void *pw, char *line);
static int FAST_FUNC bb__parsegrent(void *gr, char *line);
#if ENABLE_USE_BB_SHADOW
static int FAST_FUNC bb__parsespent(void *sp, char *line);
#endif

/**********************************************************************/
/* We avoid having big global data. */

struct statics {
	/* Smaller things first */
	/* It's ok to use one buffer for getpwuid and getpwnam. Manpage says:
	 * "The return value may point to a static area, and may be overwritten
	 * by subsequent calls to getpwent(), getpwnam(), or getpwuid()."
	 */
	struct passwd getpw_resultbuf;
	struct group getgr_resultbuf;

	char getpw_buffer[PWD_BUFFER_SIZE];
	char getgr_buffer[GRP_BUFFER_SIZE];
#if 0 //ENABLE_USE_BB_SHADOW
	struct spwd getsp_resultbuf;
	char getsp_buffer[PWD_BUFFER_SIZE];
#endif
// Not converted - too small to bother
//pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
//FILE *pwf /*= NULL*/;
//FILE *grf /*= NULL*/;
//FILE *spf /*= NULL*/;
};

static struct statics *ptr_to_statics;

static struct statics *get_S(void)
{
	if (!ptr_to_statics)
		ptr_to_statics = xzalloc(sizeof(*ptr_to_statics));
	return ptr_to_statics;
}

/* Always use in this order, get_S() must be called first */
#define RESULTBUF(name) &((S = get_S())->name##_resultbuf)
#define BUFFER(name)    (S->name##_buffer)

/**********************************************************************/
/* For the various fget??ent_r funcs, return
 *
 *  0: success
 *  ENOENT: end-of-file encountered
 *  ERANGE: buflen too small
 *  other error values possible. See bb__pgsreader.
 *
 * Also, *result == resultbuf on success and NULL on failure.
 *
 * NOTE: glibc difference - For the ENOENT case, glibc also sets errno.
 *   We do not, as it really isn't an error if we reach the end-of-file.
 *   Doing so is analogous to having fgetc() set errno on EOF.
 */
/**********************************************************************/

int fgetpwent_r(FILE *__restrict stream, struct passwd *__restrict resultbuf,
				char *__restrict buffer, size_t buflen,
				struct passwd **__restrict result)
{
	int rv;

	*result = NULL;

	rv = bb__pgsreader(bb__parsepwent, resultbuf, buffer, buflen, stream);
	if (!rv) {
		*result = resultbuf;
	}

	return rv;
}

int fgetgrent_r(FILE *__restrict stream, struct group *__restrict resultbuf,
				char *__restrict buffer, size_t buflen,
				struct group **__restrict result)
{
	int rv;

	*result = NULL;

	rv = bb__pgsreader(bb__parsegrent, resultbuf, buffer, buflen, stream);
	if (!rv) {
		*result = resultbuf;
	}

	return rv;
}

#if ENABLE_USE_BB_SHADOW
#ifdef UNUSED_FOR_NOW
int fgetspent_r(FILE *__restrict stream, struct spwd *__restrict resultbuf,
				char *__restrict buffer, size_t buflen,
				struct spwd **__restrict result)
{
	int rv;

	*result = NULL;

	rv = bb__pgsreader(bb__parsespent, resultbuf, buffer, buflen, stream);
	if (!rv) {
		*result = resultbuf;
	}

	return rv;
}
#endif
#endif

/**********************************************************************/
/* For the various fget??ent funcs, return NULL on failure and a
 * pointer to the appropriate struct (statically allocated) on success.
 * TODO: audit & stop using these in bbox, they pull in static buffers */
/**********************************************************************/

#ifdef UNUSED_SINCE_WE_AVOID_STATIC_BUFS
struct passwd *fgetpwent(FILE *stream)
{
	struct statics *S;
	struct passwd *resultbuf = RESULTBUF(getpw);
	char *buffer = BUFFER(getpw);
	struct passwd *result;

	fgetpwent_r(stream, resultbuf, buffer, sizeof(BUFFER(getpw)), &result);
	return result;
}

struct group *fgetgrent(FILE *stream)
{
	struct statics *S;
	struct group *resultbuf = RESULTBUF(getgr);
	char *buffer = BUFFER(getgr);
	struct group *result;

	fgetgrent_r(stream, resultbuf, buffer, sizeof(BUFFER(getgr)), &result);
	return result;
}
#endif

#if ENABLE_USE_BB_SHADOW
#ifdef UNUSED_SINCE_WE_AVOID_STATIC_BUFS
struct spwd *fgetspent(FILE *stream)
{
	struct statics *S;
	struct spwd *resultbuf = RESULTBUF(getsp);
	char *buffer = BUFFER(getsp);
	struct spwd *result;

	fgetspent_r(stream, resultbuf, buffer, sizeof(BUFFER(getsp)), &result);
	return result;
}
#endif

#ifdef UNUSED_FOR_NOW
int sgetspent_r(const char *string, struct spwd *result_buf,
				char *buffer, size_t buflen, struct spwd **result)
{
	int rv = ERANGE;

	*result = NULL;

	if (buflen < PWD_BUFFER_SIZE) {
 DO_ERANGE:
		errno = rv;
		goto DONE;
	}

	if (string != buffer) {
		if (strlen(string) >= buflen) {
			goto DO_ERANGE;
		}
		strcpy(buffer, string);
	}

	rv = bb__parsespent(result_buf, buffer);
	if (!rv) {
		*result = result_buf;
	}

 DONE:
	return rv;
}
#endif
#endif /* ENABLE_USE_BB_SHADOW */

/**********************************************************************/

#define GETXXKEY_R_FUNC         getpwnam_r
#define GETXXKEY_R_PARSER       bb__parsepwent
#define GETXXKEY_R_ENTTYPE      struct passwd
#define GETXXKEY_R_TEST(ENT)    (!strcmp((ENT)->pw_name, key))
#define GETXXKEY_R_KEYTYPE      const char *__restrict
#define GETXXKEY_R_PATHNAME     _PATH_PASSWD
#include "pwd_grp_internal.c"

#define GETXXKEY_R_FUNC         getgrnam_r
#define GETXXKEY_R_PARSER       bb__parsegrent
#define GETXXKEY_R_ENTTYPE      struct group
#define GETXXKEY_R_TEST(ENT)    (!strcmp((ENT)->gr_name, key))
#define GETXXKEY_R_KEYTYPE      const char *__restrict
#define GETXXKEY_R_PATHNAME     _PATH_GROUP
#include "pwd_grp_internal.c"

#if ENABLE_USE_BB_SHADOW
#define GETXXKEY_R_FUNC         getspnam_r
#define GETXXKEY_R_PARSER       bb__parsespent
#define GETXXKEY_R_ENTTYPE      struct spwd
#define GETXXKEY_R_TEST(ENT)    (!strcmp((ENT)->sp_namp, key))
#define GETXXKEY_R_KEYTYPE      const char *__restrict
#define GETXXKEY_R_PATHNAME     _PATH_SHADOW
#include "pwd_grp_internal.c"
#endif

#define GETXXKEY_R_FUNC         getpwuid_r
#define GETXXKEY_R_PARSER       bb__parsepwent
#define GETXXKEY_R_ENTTYPE      struct passwd
#define GETXXKEY_R_TEST(ENT)    ((ENT)->pw_uid == key)
#define GETXXKEY_R_KEYTYPE      uid_t
#define GETXXKEY_R_PATHNAME     _PATH_PASSWD
#include "pwd_grp_internal.c"

#define GETXXKEY_R_FUNC         getgrgid_r
#define GETXXKEY_R_PARSER       bb__parsegrent
#define GETXXKEY_R_ENTTYPE      struct group
#define GETXXKEY_R_TEST(ENT)    ((ENT)->gr_gid == key)
#define GETXXKEY_R_KEYTYPE      gid_t
#define GETXXKEY_R_PATHNAME     _PATH_GROUP
#include "pwd_grp_internal.c"

/**********************************************************************/
/* TODO: audit & stop using these in bbox, they pull in static buffers */

/* This one has many users */
struct passwd *getpwuid(uid_t uid)
{
	struct statics *S;
	struct passwd *resultbuf = RESULTBUF(getpw);
	char *buffer = BUFFER(getpw);
	struct passwd *result;

	getpwuid_r(uid, resultbuf, buffer, sizeof(BUFFER(getpw)), &result);
	return result;
}

/* This one has many users */
struct group *getgrgid(gid_t gid)
{
	struct statics *S;
	struct group *resultbuf = RESULTBUF(getgr);
	char *buffer = BUFFER(getgr);
	struct group *result;

	getgrgid_r(gid, resultbuf, buffer, sizeof(BUFFER(getgr)), &result);
	return result;
}

#if 0 //ENABLE_USE_BB_SHADOW
/* This function is non-standard and is currently not built.  It seems
 * to have been created as a reentrant version of the non-standard
 * functions getspuid.  Why getspuid was added, I do not know. */
int getspuid_r(uid_t uid, struct spwd *__restrict resultbuf,
		       char *__restrict buffer, size_t buflen,
		       struct spwd **__restrict result)
{
	int rv;
	struct passwd *pp;
	struct passwd password;
	char pwd_buff[PWD_BUFFER_SIZE];

	*result = NULL;
	rv = getpwuid_r(uid, &password, pwd_buff, sizeof(pwd_buff), &pp);
	if (!rv) {
		rv = getspnam_r(password.pw_name, resultbuf, buffer, buflen, result);
	}

	return rv;
}

/* This function is non-standard and is currently not built.
 * Why it was added, I do not know. */
struct spwd *getspuid(uid_t uid)
{
	struct statics *S;
	struct spwd *resultbuf = RESULTBUF(getsp);
	char *buffer = BUFFER(getsp);
	struct spwd *result;

	getspuid_r(uid, resultbuf, buffer, sizeof(BUFFER(getsp)), &result);
	return result;
}
#endif

/* This one has many users */
struct passwd *getpwnam(const char *name)
{
	struct statics *S;
	struct passwd *resultbuf = RESULTBUF(getpw);
	char *buffer = BUFFER(getpw);
	struct passwd *result;

	getpwnam_r(name, resultbuf, buffer, sizeof(BUFFER(getpw)), &result);
	return result;
}

/* This one has many users */
struct group *getgrnam(const char *name)
{
	struct statics *S;
	struct group *resultbuf = RESULTBUF(getgr);
	char *buffer = BUFFER(getgr);
	struct group *result;

	getgrnam_r(name, resultbuf, buffer, sizeof(BUFFER(getgr)), &result);
	return result;
}

#if 0 //ENABLE_USE_BB_SHADOW
struct spwd *getspnam(const char *name)
{
	struct statics *S;
	struct spwd *resultbuf = RESULTBUF(getsp);
	char *buffer = BUFFER(getsp);
	struct spwd *result;

	getspnam_r(name, resultbuf, buffer, sizeof(BUFFER(getsp)), &result);
	return result;
}
#endif

/**********************************************************************/

/* FIXME: we don't have such CONFIG_xx - ?! */

#if defined CONFIG_USE_BB_THREADSAFE_SHADOW && defined PTHREAD_MUTEX_INITIALIZER
static pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
# define LOCK		pthread_mutex_lock(&mylock)
# define UNLOCK		pthread_mutex_unlock(&mylock);
#else
# define LOCK		((void) 0)
# define UNLOCK		((void) 0)
#endif

static FILE *pwf /*= NULL*/;
void setpwent(void)
{
	LOCK;
	if (pwf) {
		rewind(pwf);
	}
	UNLOCK;
}

void endpwent(void)
{
	LOCK;
	if (pwf) {
		fclose(pwf);
		pwf = NULL;
	}
	UNLOCK;
}


int getpwent_r(struct passwd *__restrict resultbuf,
			   char *__restrict buffer, size_t buflen,
			   struct passwd **__restrict result)
{
	int rv;

	LOCK;
	*result = NULL;				/* In case of error... */

	if (!pwf) {
		pwf = fopen_for_read(_PATH_PASSWD);
		if (!pwf) {
			rv = errno;
			goto ERR;
		}
		close_on_exec_on(fileno(pwf));
	}

	rv = bb__pgsreader(bb__parsepwent, resultbuf, buffer, buflen, pwf);
	if (!rv) {
		*result = resultbuf;
	}

 ERR:
	UNLOCK;
	return rv;
}

static FILE *grf /*= NULL*/;
void setgrent(void)
{
	LOCK;
	if (grf) {
		rewind(grf);
	}
	UNLOCK;
}

void endgrent(void)
{
	LOCK;
	if (grf) {
		fclose(grf);
		grf = NULL;
	}
	UNLOCK;
}

int getgrent_r(struct group *__restrict resultbuf,
			   char *__restrict buffer, size_t buflen,
			   struct group **__restrict result)
{
	int rv;

	LOCK;
	*result = NULL;				/* In case of error... */

	if (!grf) {
		grf = fopen_for_read(_PATH_GROUP);
		if (!grf) {
			rv = errno;
			goto ERR;
		}
		close_on_exec_on(fileno(grf));
	}

	rv = bb__pgsreader(bb__parsegrent, resultbuf, buffer, buflen, grf);
	if (!rv) {
		*result = resultbuf;
	}

 ERR:
	UNLOCK;
	return rv;
}

#ifdef UNUSED_FOR_NOW
#if ENABLE_USE_BB_SHADOW
static FILE *spf /*= NULL*/;
void setspent(void)
{
	LOCK;
	if (spf) {
		rewind(spf);
	}
	UNLOCK;
}

void endspent(void)
{
	LOCK;
	if (spf) {
		fclose(spf);
		spf = NULL;
	}
	UNLOCK;
}

int getspent_r(struct spwd *resultbuf, char *buffer,
			   size_t buflen, struct spwd **result)
{
	int rv;

	LOCK;
	*result = NULL;				/* In case of error... */

	if (!spf) {
		spf = fopen_for_read(_PATH_SHADOW);
		if (!spf) {
			rv = errno;
			goto ERR;
		}
		close_on_exec_on(fileno(spf));
	}

	rv = bb__pgsreader(bb__parsespent, resultbuf, buffer, buflen, spf);
	if (!rv) {
		*result = resultbuf;
	}

 ERR:
	UNLOCK;
	return rv;
}
#endif
#endif /* UNUSED_FOR_NOW */

#ifdef UNUSED_SINCE_WE_AVOID_STATIC_BUFS
struct passwd *getpwent(void)
{
	static char line_buff[PWD_BUFFER_SIZE];
	static struct passwd pwd;
	struct passwd *result;

	getpwent_r(&pwd, line_buff, sizeof(line_buff), &result);
	return result;
}

struct group *getgrent(void)
{
	static char line_buff[GRP_BUFFER_SIZE];
	static struct group gr;
	struct group *result;

	getgrent_r(&gr, line_buff, sizeof(line_buff), &result);
	return result;
}

#if ENABLE_USE_BB_SHADOW
struct spwd *getspent(void)
{
	static char line_buff[PWD_BUFFER_SIZE];
	static struct spwd spwd;
	struct spwd *result;

	getspent_r(&spwd, line_buff, sizeof(line_buff), &result);
	return result;
}

struct spwd *sgetspent(const char *string)
{
	static char line_buff[PWD_BUFFER_SIZE];
	static struct spwd spwd;
	struct spwd *result;

	sgetspent_r(string, &spwd, line_buff, sizeof(line_buff), &result);
	return result;
}
#endif
#endif /* UNUSED_SINCE_WE_AVOID_STATIC_BUFS */

static gid_t *getgrouplist_internal(int *ngroups_ptr, const char *user, gid_t gid)
{
	FILE *grfile;
	gid_t *group_list;
	int ngroups;
	struct group group;
	char buff[PWD_BUFFER_SIZE];

	/* We alloc space for 8 gids at a time. */
	group_list = xmalloc(8 * sizeof(group_list[0]));
	group_list[0] = gid;
	ngroups = 1;

	grfile = fopen_for_read(_PATH_GROUP);
	if (grfile) {
		while (!bb__pgsreader(bb__parsegrent, &group, buff, sizeof(buff), grfile)) {
			char **m;
			assert(group.gr_mem); /* Must have at least a NULL terminator. */
			if (group.gr_gid == gid)
				continue;
			for (m = group.gr_mem; *m; m++) {
				if (strcmp(*m, user) != 0)
					continue;
				group_list = xrealloc_vector(group_list, /*8=2^3:*/ 3, ngroups);
				group_list[ngroups++] = group.gr_gid;
				break;
			}
		}
		fclose(grfile);
	}
	*ngroups_ptr = ngroups;
	return group_list;
}

int initgroups(const char *user, gid_t gid)
{
	int ngroups;
	gid_t *group_list = getgrouplist_internal(&ngroups, user, gid);

	ngroups = setgroups(ngroups, group_list);
	free(group_list);
	return ngroups;
}

int getgrouplist(const char *user, gid_t gid, gid_t *groups, int *ngroups)
{
	int ngroups_old = *ngroups;
	gid_t *group_list = getgrouplist_internal(ngroups, user, gid);

	if (*ngroups <= ngroups_old) {
		ngroups_old = *ngroups;
		memcpy(groups, group_list, ngroups_old * sizeof(groups[0]));
	} else {
		ngroups_old = -1;
	}
	free(group_list);
	return ngroups_old;
}

#ifdef UNUSED_SINCE_WE_AVOID_STATIC_BUFS
int putpwent(const struct passwd *__restrict p, FILE *__restrict f)
{
	int rv = -1;

#if 0
	/* glibc does this check */
	if (!p || !f) {
		errno = EINVAL;
		return rv;
	}
#endif

	/* No extra thread locking is needed above what fprintf does. */
	if (fprintf(f, "%s:%s:%lu:%lu:%s:%s:%s\n",
				p->pw_name, p->pw_passwd,
				(unsigned long)(p->pw_uid),
				(unsigned long)(p->pw_gid),
				p->pw_gecos, p->pw_dir, p->pw_shell) >= 0
		) {
		rv = 0;
	}

	return rv;
}

int putgrent(const struct group *__restrict p, FILE *__restrict f)
{
	int rv = -1;

#if 0
	/* glibc does this check */
	if (!p || !f) {
		errno = EINVAL;
		return rv;
	}
#endif

	if (fprintf(f, "%s:%s:%lu:",
				p->gr_name, p->gr_passwd,
				(unsigned long)(p->gr_gid)) >= 0
	) {
		static const char format[] ALIGN1 = ",%s";

		char **m;
		const char *fmt;

		fmt = format + 1;

		assert(p->gr_mem);
		m = p->gr_mem;

		while (1) {
			if (!*m) {
				if (fputc('\n', f) >= 0) {
					rv = 0;
				}
				break;
			}
			if (fprintf(f, fmt, *m) < 0) {
				break;
			}
			m++;
			fmt = format;
		}
	}

	return rv;
}
#endif

#if ENABLE_USE_BB_SHADOW
#ifdef UNUSED_FOR_NOW
static const unsigned char put_sp_off[] ALIGN1 = {
	offsetof(struct spwd, sp_lstchg),       /* 2 - not a char ptr */
	offsetof(struct spwd, sp_min),          /* 3 - not a char ptr */
	offsetof(struct spwd, sp_max),          /* 4 - not a char ptr */
	offsetof(struct spwd, sp_warn),         /* 5 - not a char ptr */
	offsetof(struct spwd, sp_inact),        /* 6 - not a char ptr */
	offsetof(struct spwd, sp_expire)        /* 7 - not a char ptr */
};

int putspent(const struct spwd *p, FILE *stream)
{
	const char *fmt;
	long x;
	int i;
	int rv = -1;

	/* Unlike putpwent and putgrent, glibc does not check the args. */
	if (fprintf(stream, "%s:%s:", p->sp_namp,
				(p->sp_pwdp ? p->sp_pwdp : "")) < 0
	) {
		goto DO_UNLOCK;
	}

	for (i = 0; i < sizeof(put_sp_off); i++) {
		fmt = "%ld:";
		x = *(long *)((char *)p + put_sp_off[i]);
		if (x == -1) {
			fmt += 3;
		}
		if (fprintf(stream, fmt, x) < 0) {
			goto DO_UNLOCK;
		}
	}

	if ((p->sp_flag != ~0UL) && (fprintf(stream, "%lu", p->sp_flag) < 0)) {
		goto DO_UNLOCK;
	}

	if (fputc('\n', stream) > 0) {
		rv = 0;
	}

 DO_UNLOCK:
	return rv;
}
#endif
#endif /* USE_BB_SHADOW */

/**********************************************************************/
/* Internal functions                                                 */
/**********************************************************************/

static const unsigned char pw_off[] ALIGN1 = {
	offsetof(struct passwd, pw_name),       /* 0 */
	offsetof(struct passwd, pw_passwd),     /* 1 */
	offsetof(struct passwd, pw_uid),        /* 2 - not a char ptr */
	offsetof(struct passwd, pw_gid),        /* 3 - not a char ptr */
	offsetof(struct passwd, pw_gecos),      /* 4 */
	offsetof(struct passwd, pw_dir),        /* 5 */
	offsetof(struct passwd, pw_shell)       /* 6 */
};

static int FAST_FUNC bb__parsepwent(void *data, char *line)
{
	char *endptr;
	char *p;
	int i;

	i = 0;
	while (1) {
		p = (char *) data + pw_off[i];

		if (i < 2 || i > 3) {
			*((char **) p) = line;
			if (i == 6) {
				return 0;
			}
			/* NOTE: glibc difference - glibc allows omission of
			 * ':' seperators after the gid field if all remaining
			 * entries are empty.  We require all separators. */
			line = strchr(line, ':');
			if (!line) {
				break;
			}
		} else {
			unsigned long t = strtoul(line, &endptr, 10);
			/* Make sure we had at least one digit, and that the
			 * failing char is the next field seperator ':'.  See
			 * glibc difference note above. */
			/* TODO: Also check for leading whitespace? */
			if ((endptr == line) || (*endptr != ':')) {
				break;
			}
			line = endptr;
			if (i & 1) {		/* i == 3 -- gid */
				*((gid_t *) p) = t;
			} else {			/* i == 2 -- uid */
				*((uid_t *) p) = t;
			}
		}

		*line++ = '\0';
		i++;
	} /* while (1) */

	return -1;
}

/**********************************************************************/

static const unsigned char gr_off[] ALIGN1 = {
	offsetof(struct group, gr_name),        /* 0 */
	offsetof(struct group, gr_passwd),      /* 1 */
	offsetof(struct group, gr_gid)          /* 2 - not a char ptr */
};

static int FAST_FUNC bb__parsegrent(void *data, char *line)
{
	char *endptr;
	char *p;
	int i;
	char **members;
	char *end_of_buf;

	end_of_buf = ((struct group *) data)->gr_name; /* Evil hack! */
	i = 0;
	while (1) {
		p = (char *) data + gr_off[i];

		if (i < 2) {
			*((char **) p) = line;
			line = strchr(line, ':');
			if (!line) {
				break;
			}
			*line++ = '\0';
			i++;
		} else {
			*((gid_t *) p) = strtoul(line, &endptr, 10);

			/* NOTE: glibc difference - glibc allows omission of the
			 * trailing colon when there is no member list.  We treat
			 * this as an error. */

			/* Make sure we had at least one digit, and that the
			 * failing char is the next field seperator ':'.  See
			 * glibc difference note above. */
			if ((endptr == line) || (*endptr != ':')) {
				break;
			}

			i = 1;				/* Count terminating NULL ptr. */
			p = endptr;

			if (p[1]) { /* We have a member list to process. */
				/* Overwrite the last ':' with a ',' before counting.
				 * This allows us to (1) test for initial ','
				 * and (2) adds one ',' so that the number of commas
				 * equals the member count. */
				*p = ',';
				do {
					/* NOTE: glibc difference - glibc allows and trims leading
					 * (but not trailing) space.  We treat this as an error. */
					/* NOTE: glibc difference - glibc allows consecutive and
					 * trailing commas, and ignores "empty string" users.  We
					 * treat this as an error. */
					if (*p == ',') {
						++i;
						*p = 0;	/* nul-terminate each member string. */
						if (!*++p || (*p == ',') || isspace(*p)) {
							goto ERR;
						}
					}
				} while (*++p);
			}

			/* Now align (p+1), rounding up. */
			/* Assumes sizeof(char **) is a power of 2. */
			members = (char **)( (((intptr_t) p) + sizeof(char **))
								 & ~((intptr_t)(sizeof(char **) - 1)) );

			if (((char *)(members + i)) > end_of_buf) {	/* No space. */
				break;
			}

			((struct group *) data)->gr_mem = members;

			if (--i) {
				p = endptr;	/* Pointing to char prior to first member. */
				while (1) {
					*members++ = ++p;
					if (!--i)
						break;
					while (*++p)
						continue;
				}
			}
			*members = NULL;

			return 0;
		}
	} /* while (1) */

 ERR:
	return -1;
}

/**********************************************************************/

#if ENABLE_USE_BB_SHADOW
static const unsigned char sp_off[] ALIGN1 = {
	offsetof(struct spwd, sp_namp),         /* 0: char* */
	offsetof(struct spwd, sp_pwdp),         /* 1: char* */
	offsetof(struct spwd, sp_lstchg),       /* 2: long */
	offsetof(struct spwd, sp_min),          /* 3: long */
	offsetof(struct spwd, sp_max),          /* 4: long */
	offsetof(struct spwd, sp_warn),         /* 5: long */
	offsetof(struct spwd, sp_inact),        /* 6: long */
	offsetof(struct spwd, sp_expire),       /* 7: long */
	offsetof(struct spwd, sp_flag)          /* 8: unsigned long */
};

static int FAST_FUNC bb__parsespent(void *data, char *line)
{
	char *endptr;
	char *p;
	int i;

	i = 0;
	while (1) {
		p = (char *) data + sp_off[i];
		if (i < 2) {
			*((char **) p) = line;
			line = strchr(line, ':');
			if (!line) {
				break; /* error */
			}
		} else {
			*((long *) p) = strtoul(line, &endptr, 10);
			if (endptr == line) {
				*((long *) p) = -1L;
			}
			line = endptr;
			if (i == 8) {
				if (*line != '\0') {
					break; /* error */
				}
				return 0; /* all ok */
			}
			if (*line != ':') {
				break; /* error */
			}
		}
		*line++ = '\0';
		i++;
	}

	return EINVAL;
}
#endif

/**********************************************************************/

/* Reads until EOF, or until it finds a line which fits in the buffer
 * and for which the parser function succeeds.
 *
 * Returns 0 on success and ENOENT for end-of-file (glibc convention).
 */
static int bb__pgsreader(
		int FAST_FUNC (*parserfunc)(void *d, char *line),
		void *data,
		char *__restrict line_buff,
		size_t buflen,
		FILE *f)
{
	int skip;
	int rv = ERANGE;

	if (buflen < PWD_BUFFER_SIZE) {
		errno = rv;
		return rv;
	}

	skip = 0;
	while (1) {
		if (!fgets(line_buff, buflen, f)) {
			if (feof(f)) {
				rv = ENOENT;
			}
			break;
		}

		{
			int line_len = strlen(line_buff) - 1;
			if (line_len >= 0 && line_buff[line_len] == '\n') {
				line_buff[line_len] = '\0';
			} else
			if (line_len + 2 == buflen) {
				/* A start (or continuation) of overlong line */
				skip = 1;
				continue;
			} /* else: a last line in the file, and it has no '\n' */
		}

		if (skip) {
			/* This "line" is a remainder of overlong line, ignore */
			skip = 0;
			continue;
		}

		/* NOTE: glibc difference - glibc strips leading whitespace from
		 * records.  We do not allow leading whitespace. */

		/* Skip empty lines, comment lines, and lines with leading
		 * whitespace. */
		if (line_buff[0] != '\0' && line_buff[0] != '#' && !isspace(line_buff[0])) {
			if (parserfunc == bb__parsegrent) {
				/* Do evil group hack:
				 * The group entry parsing function needs to know where
				 * the end of the buffer is so that it can construct the
				 * group member ptr table. */
				((struct group *) data)->gr_name = line_buff + buflen;
			}
			if (parserfunc(data, line_buff) == 0) {
				rv = 0;
				break;
			}
		}
	} /* while (1) */

	return rv;
}

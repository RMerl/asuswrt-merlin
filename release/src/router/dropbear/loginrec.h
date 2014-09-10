#ifndef _HAVE_LOGINREC_H_
#define _HAVE_LOGINREC_H_

/*
 * Copyright (c) 2000 Andre Lucas.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 ** loginrec.h:  platform-independent login recording and lastlog retrieval
 **/

#include "includes.h"

/* RCSID("Id: loginrec.h,v 1.2 2004/05/04 10:17:43 matt Exp "); */

/* The following #defines are from OpenSSH's defines.h, required for loginrec */

/* FIXME: put default paths back in */
#ifndef UTMP_FILE
#  ifdef _PATH_UTMP
#    define UTMP_FILE _PATH_UTMP
#  else
#    ifdef CONF_UTMP_FILE
#      define UTMP_FILE CONF_UTMP_FILE
#    endif
#  endif
#endif
#ifndef WTMP_FILE
#  ifdef _PATH_WTMP
#    define WTMP_FILE _PATH_WTMP
#  else
#    ifdef CONF_WTMP_FILE
#      define WTMP_FILE CONF_WTMP_FILE
#    endif
#  endif
#endif
/* pick up the user's location for lastlog if given */
#ifndef LASTLOG_FILE
#  ifdef _PATH_LASTLOG
#    define LASTLOG_FILE _PATH_LASTLOG
#  else
#    ifdef CONF_LASTLOG_FILE
#      define LASTLOG_FILE CONF_LASTLOG_FILE
#    endif
#  endif
#endif


/* The login() library function in libutil is first choice */
#if defined(HAVE_LOGIN) && !defined(DISABLE_LOGIN)
#  define USE_LOGIN

#else
/* Simply select your favourite login types. */
/* Can't do if-else because some systems use several... <sigh> */
#  if defined(HAVE_UTMPX_H) && defined(UTMPX_FILE) && !defined(DISABLE_UTMPX)
#    define USE_UTMPX
#  endif
#  if defined(HAVE_UTMP_H) && defined(UTMP_FILE) && !defined(DISABLE_UTMP)
#    define USE_UTMP
#  endif
#  if defined(WTMPX_FILE) && !defined(DISABLE_WTMPX)
#    define USE_WTMPX
#  endif
#  if defined(WTMP_FILE) && !defined(DISABLE_WTMP)
#    define USE_WTMP
#  endif

#endif

/* I hope that the presence of LASTLOG_FILE is enough to detect this */
#if defined(LASTLOG_FILE) && !defined(DISABLE_LASTLOG)
#  define USE_LASTLOG
#endif


/**
 ** you should use the login_* calls to work around platform dependencies
 **/

/*
 * login_netinfo structure
 */

union login_netinfo {
	struct sockaddr sa;
	struct sockaddr_in sa_in;
#ifdef HAVE_STRUCT_SOCKADDR_STORAGE
	struct sockaddr_storage sa_storage;
#endif
};

/*
 *   * logininfo structure  *
 */
/* types - different to utmp.h 'type' macros */
/* (though set to the same value as linux, openbsd and others...) */
#define LTYPE_LOGIN    7
#define LTYPE_LOGOUT   8

/* string lengths - set very long */
#define LINFO_PROGSIZE 64
#define LINFO_LINESIZE 64
#define LINFO_NAMESIZE 64
#define LINFO_HOSTSIZE 256

struct logininfo {
	char       progname[LINFO_PROGSIZE];     /* name of program (for PAM) */
	int        progname_null;
	short int  type;                         /* type of login (LTYPE_*) */
	int        pid;                          /* PID of login process */
	int        uid;                          /* UID of this user */
	char       line[LINFO_LINESIZE];         /* tty/pty name */
	char       username[LINFO_NAMESIZE];     /* login username */
	char       hostname[LINFO_HOSTSIZE];     /* remote hostname */
	/* 'exit_status' structure components */
	int        exit;                        /* process exit status */
	int        termination;                 /* process termination status */
	/* struct timeval (sys/time.h) isn't always available, if it isn't we'll
	 * use time_t's value as tv_sec and set tv_usec to 0
	 */
	unsigned int tv_sec;
	unsigned int tv_usec;
	union login_netinfo hostaddr;       /* caller's host address(es) */
}; /* struct logininfo */

/*
 * login recording functions
 */

/** 'public' functions */

struct logininfo *login_alloc_entry(int pid, const char *username,
				    const char *hostname, const char *line);
/* free a structure */
void login_free_entry(struct logininfo *li);
/* fill out a pre-allocated structure with useful information */
int login_init_entry(struct logininfo *li, int pid, const char *username,
		     const char *hostname, const char *line);
/* place the current time in a logininfo struct */
void login_set_current_time(struct logininfo *li);

/* record the entry */
int login_login (struct logininfo *li);
int login_logout(struct logininfo *li);
#ifdef LOGIN_NEEDS_UTMPX
int login_utmp_only(struct logininfo *li);
#endif

/** End of public functions */

/* record the entry */
int login_write (struct logininfo *li);
int login_log_entry(struct logininfo *li);

/* set the network address based on network address type */
void login_set_addr(struct logininfo *li, const struct sockaddr *sa,
		    const unsigned int sa_size);

/* produce various forms of the line filename */
char *line_fullname(char *dst, const char *src, size_t dstsize);
char *line_stripname(char *dst, const char *src, size_t dstsize);
char *line_abbrevname(char *dst, const char *src, size_t dstsize);

#endif /* _HAVE_LOGINREC_H_ */

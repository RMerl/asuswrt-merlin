#ifndef _system_passwd_h
#define _system_passwd_h

/*
   Unix SMB/CIFS implementation.

   passwd system include wrappers

   Copyright (C) Andrew Tridgell 2004

     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

*/

/* this needs to be included before nss_wrapper.h on some systems */
#include <unistd.h>

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_SYS_PRIV_H
#include <sys/priv.h>
#endif
#ifdef HAVE_SYS_ID_H
#include <sys/id.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#ifdef HAVE_SYS_SECURITY_H
#include <sys/security.h>
#include <prot.h>
#define PASSWORD_LENGTH 16
#endif  /* HAVE_SYS_SECURITY_H */

#ifdef HAVE_GETPWANAM
#include <sys/label.h>
#include <sys/audit.h>
#include <pwdadj.h>
#endif

#ifdef HAVE_COMPAT_H
#include <compat.h>
#endif

#ifdef REPLACE_GETPASS
#if defined(REPLACE_GETPASS_BY_GETPASSPHRASE)
#define getpass(prompt) getpassphrase(prompt)
#else
#define getpass(prompt) rep_getpass(prompt)
char *rep_getpass(const char *prompt);
#endif
#endif

#ifndef NGROUPS_MAX
#define NGROUPS_MAX 32 /* Guess... */
#endif

/* what is the longest significant password available on your system?
 Knowing this speeds up password searches a lot */
#ifndef PASSWORD_LENGTH
#define PASSWORD_LENGTH 8
#endif

#if defined(HAVE_PUTPRPWNAM) && defined(AUTH_CLEARTEXT_SEG_CHARS)
#define OSF1_ENH_SEC 1
#endif

#ifndef ALLOW_CHANGE_PASSWORD
#if (defined(HAVE_TERMIOS_H) && defined(HAVE_DUP2) && defined(HAVE_SETSID))
#define ALLOW_CHANGE_PASSWORD 1
#endif
#endif

#if defined(HAVE_CRYPT16) && defined(HAVE_GETAUTHUID)
#define ULTRIX_AUTH 1
#endif

#ifdef NSS_WRAPPER
#ifndef NSS_WRAPPER_NOT_REPLACE
#define NSS_WRAPPER_REPLACE
#endif
#include "../nss_wrapper/nss_wrapper.h"
#endif

#endif

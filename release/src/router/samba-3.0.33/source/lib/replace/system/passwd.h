#ifndef _system_passwd_h
#define _system_passwd_h

/* 
   Unix SMB/CIFS implementation.

   passwd system include wrappers

   Copyright (C) Andrew Tridgell 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

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
#define getpass(prompt) getsmbpass((prompt))
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

#endif

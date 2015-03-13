/* Configuration header file for MS-DOS/Watt-32
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

   This file is part of GNU Wget.

   GNU Wget is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   GNU Wget is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Wget.  If not, see <http://www.gnu.org/licenses/>.

   Additional permission under GNU GPL version 3 section 7

   If you modify this program, or any covered work, by linking or
   combining it with the OpenSSL project's OpenSSL library (or a
   modified version of that library), containing parts covered by the
   terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
   grants you additional permission to convey the resulting work.
   Corresponding Source for a non-source form of such a combination
   shall include the source code for the parts of OpenSSL used as well
   as that of the covered work.  */


#ifndef CONFIG_DOS_H
#define CONFIG_DOS_H

#include <stdlib.h>
#include <limits.h>
#include <tcp.h>
#include <malloc.h>

#ifdef __DJGPP__
#include <sys/config.h>
#endif

#include <sys/errno.h>

#if defined(__WATCOMC__)
  #if (__WATCOMC__ >= 1250)  /* OW 1.5+ */
  #define OPENWATCOM_15
  #endif
  #if (__WATCOMC__ >= 1270)  /* OW 1.7+ */
    #define OPENWATCOM_17
  #endif
#endif

#if defined(__HIGHC__)
  #define HAVE_UNISTD_H 1
  #define HAVE_UTIME_H 1
#endif

#if defined(__WATCOMC__) || defined(__BORLANDC__) || defined(__HIGHC__)
  #define inline
#endif

#define USE_OPIE 1
#define USE_DIGEST 1
#define DEBUG

#ifdef __DJGPP__
  #define HAVE__BOOL          1
  #define HAVE_STRCASECMP 1
  #define HAVE_STRNCASECMP 1
  #define HAVE_SIGSETJMP 1
  #define HAVE_SIGBLOCK 1
  #define HAVE_STRUCT_UTIMBUF 1
  #define HAVE_SYS_SELECT_H   1
  #define HAVE_USLEEP         1
  #define HAVE_UTIME_H        1
  #define HAVE_INT64_T        1

  #if (DJGPP_MINOR >= 4)
    #define HAVE_STDBOOL_H 1
    #define HAVE_STDINT_H  1
    #define HAVE_SNPRINTF 1
    #define HAVE_VSNPRINTF 1
    #define HAVE_UINT32_T 1
  #endif
#endif

#ifdef __HIGHC__
  #define HAVE_STRUCT_UTIMBUF 1
  #define HAVE_UTIME_H 1
#endif

#ifdef OPENWATCOM_15
  #define HAVE_ALLOCA_H    1
  #define HAVE_INT64_T     1
  #define HAVE_SNPRINTF    1
  #define HAVE_STRCASECMP  1
  #define HAVE_STRNCASECMP 1
  #define HAVE_STDINT_H    1
  #define HAVE_UTIME_H     1
#endif

#ifdef OPENWATCOM_17
  #define HAVE__BOOL       1
  #define HAVE_STDBOOL_H   1
#endif

#define HAVE_PROCESS_H     1
#define HAVE_STRDUP 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_BUILTIN_MD5 1
#define HAVE_ISATTY 1

#define lookup_host  wget_lookuphost
#define select       select_s
#define socklen_t    int

#define sock_read    wget_sock_read
#define sock_write   wget_sock_write
#define sock_close   wget_sock_close

#if !defined(__DJGPP__)
  #include <direct.h>
  #define mkdir(p,a)  (mkdir)(p)
  #define strcasecmp stricmp
#endif

#if !defined(MSDOS)
  #define MSDOS
#endif

#if !defined(USE_WATT32)
  #define USE_WATT32
#endif

#define LOCALEDIR ""
#define OS_TYPE "DOS"

#endif  /* CONFIG_DOS_H */

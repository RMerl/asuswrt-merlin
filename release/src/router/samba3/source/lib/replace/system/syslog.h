#ifndef _system_syslog_h
#define _system_syslog_h
/* 
   Unix SMB/CIFS implementation.

   syslog system include wrappers

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

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
#ifdef HAVE_SYS_SYSLOG_H
#include <sys/syslog.h>
#endif
#endif

/* For sys_adminlog(). */
#ifndef LOG_EMERG
#define LOG_EMERG       0       /* system is unusable */
#endif

#ifndef LOG_ALERT
#define LOG_ALERT       1       /* action must be taken immediately */
#endif

#ifndef LOG_CRIT
#define LOG_CRIT        2       /* critical conditions */
#endif

#ifndef LOG_ERR
#define LOG_ERR         3       /* error conditions */
#endif

#ifndef LOG_WARNING
#define LOG_WARNING     4       /* warning conditions */
#endif

#ifndef LOG_NOTICE
#define LOG_NOTICE      5       /* normal but significant condition */
#endif

#ifndef LOG_INFO
#define LOG_INFO        6       /* informational */
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG       7       /* debug-level messages */
#endif

#endif

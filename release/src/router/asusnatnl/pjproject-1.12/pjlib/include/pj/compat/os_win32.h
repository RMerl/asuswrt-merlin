/* $Id: os_win32.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_COMPAT_OS_WIN32_H__
#define __PJ_COMPAT_OS_WIN32_H__

/**
 * @file os_win32.h
 * @brief Describes Win32 operating system family specifics.
 */

#define PJ_OS_NAME		    "win32"

#define WIN32_LEAN_AND_MEAN
#define PJ_WIN32_WINNT		    0x0400
#ifndef _WIN32_WINNT
#  define _WIN32_WINNT		    PJ_WIN32_WINNT
#endif

#define PJ_HAS_ARPA_INET_H	    0
#define PJ_HAS_ASSERT_H		    1
#define PJ_HAS_CTYPE_H		    1
#define PJ_HAS_ERRNO_H		    0   /* Must be zero, otherwise errno_test() fails. */
#define PJ_HAS_LINUX_SOCKET_H	    0
#define PJ_HAS_MALLOC_H		    1
#define PJ_HAS_NETDB_H		    0
#define PJ_HAS_NETINET_IN_H	    0
#define PJ_HAS_NETINET_TCP_H	    0
#define PJ_HAS_SETJMP_H		    1
#define PJ_HAS_STDARG_H		    1
#define PJ_HAS_STDDEF_H		    1
#undef  PJ_HAS_STDINT_H
#define PJ_HAS_STDIO_H		    1
#define PJ_HAS_STDLIB_H		    1
#define PJ_HAS_STRING_H		    1
#define PJ_HAS_SYS_IOCTL_H	    0
#define PJ_HAS_SYS_SELECT_H	    0
#define PJ_HAS_SYS_SOCKET_H	    0
#define PJ_HAS_SYS_TIME_H	    0
#define PJ_HAS_SYS_TIMEB_H	    1
#define PJ_HAS_SYS_TYPES_H	    1
#define PJ_HAS_TIME_H		    1
#define PJ_HAS_UNISTD_H		    0

#define PJ_HAS_MSWSOCK_H	    1
#define PJ_HAS_WINSOCK_H	    0
#define PJ_HAS_WINSOCK2_H	    1
#define PJ_HAS_WS2TCPIP_H	    1

#define PJ_SOCK_HAS_INET_ATON	    0

/* Set 1 if native sockaddr_in has sin_len member. 
 * Default: 0
 */
#define PJ_SOCKADDR_HAS_LEN	    0

/* Is errno a good way to retrieve OS errors? (No)
 */
#define PJ_HAS_ERRNO_VAR	    0

/* When this macro is set, getsockopt(SOL_SOCKET, SO_ERROR) will return
 * the status of non-blocking connect() operation.
 */
#define PJ_HAS_SO_ERROR             1

/* This value specifies the value set in errno by the OS when a non-blocking
 * socket recv() or send() can not return immediately.
 */
#define PJ_BLOCKING_ERROR_VAL       WSAEWOULDBLOCK

/* This value specifies the value set in errno by the OS when a non-blocking
 * socket connect() can not get connected immediately.
 */
#define PJ_BLOCKING_CONNECT_ERROR_VAL   WSAEWOULDBLOCK

/**
 * If this macro is set, it tells select I/O Queue that select() needs to
 * be given correct value of nfds (i.e. largest fd + 1). This requires
 * select ioqueue to re-scan the descriptors on each registration and
 * unregistration.
 * If this macro is not set, then ioqueue will always give FD_SETSIZE for
 * nfds argument when calling select().
 *
 * Default: 0
 */
#define PJ_SELECT_NEEDS_NFDS		0

/* Default threading is enabled, unless it's overridden. */
#ifndef PJ_HAS_THREADS
#  define PJ_HAS_THREADS		(1)
#endif

#define PJ_HAS_HIGH_RES_TIMER		1
#define PJ_HAS_MALLOC			1
#ifndef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK	0
#endif

#ifdef UNICODE
#   define PJ_NATIVE_STRING_IS_UNICODE    1
#else
#   define PJ_NATIVE_STRING_IS_UNICODE    0
#endif

#define PJ_ATOMIC_VALUE_TYPE		long

/* If 1, use Read/Write mutex emulation for platforms that don't support it */
#define PJ_EMULATE_RWMUTEX		1

/* If 1, pj_thread_create() should enforce the stack size when creating 
 * threads.
 * Default: 0 (let OS decide the thread's stack size).
 */
#define PJ_THREAD_SET_STACK_SIZE    	0

/* If 1, pj_thread_create() should allocate stack from the pool supplied.
 * Default: 0 (let OS allocate memory for thread's stack).
 */
#define PJ_THREAD_ALLOCATE_STACK    	0


#endif	/* __PJ_COMPAT_OS_WIN32_H__ */


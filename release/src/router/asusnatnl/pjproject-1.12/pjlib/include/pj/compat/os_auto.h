/* pjlib/include/pj/compat/os_auto.h.  Generated from os_auto.h.in by configure.  */
/* $Id: os_auto.h.in 3692 2011-08-11 08:45:38Z ming $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_COMPAT_OS_AUTO_H__
#define __PJ_COMPAT_OS_AUTO_H__

/**
 * @file os_auto.h
 * @brief Describes operating system specifics (automatically detected by
 *        autoconf)
 */

/* Canonical OS name */
#define PJ_OS_NAME "arm-unknown-linux-gnu"

/* Legacy macros */
/* #undef PJ_WIN32 */
/* #undef PJ_WIN32_WINNT */
/* #undef WIN32_LEAN_AND_MEAN */
/* #undef PJ_DARWINOS */
#define PJ_LINUX 1
/* #undef PJ_RTEMS */
/* #undef PJ_SUNOS */

#if defined(PJ_WIN32_WINNT) && !defined(_WIN32_WINNT)
#  define _WIN32_WINNT	PJ_WIN32_WINNT
#endif

/* Headers availability */
#define PJ_HAS_ARPA_INET_H 1
#define PJ_HAS_ASSERT_H 1
#define PJ_HAS_CTYPE_H 1
#define PJ_HAS_ERRNO_H 1
#define PJ_HAS_FCNTL_H 1
#define PJ_HAS_LIMITS_H 1
#define PJ_HAS_LINUX_SOCKET_H 1
#define PJ_HAS_MALLOC_H 1
#define PJ_HAS_NETDB_H 1
#define PJ_HAS_NETINET_IN_SYSTM_H 1
#define PJ_HAS_NETINET_IN_H 1
#define PJ_HAS_NETINET_IP_H 1
#define PJ_HAS_NETINET_TCP_H 1
#define PJ_HAS_NET_IF_H 1
/* #undef PJ_HAS_IFADDRS_H */
#define PJ_HAS_SEMAPHORE_H 1
#define PJ_HAS_SETJMP_H 1
#define PJ_HAS_STDARG_H 1
#define PJ_HAS_STDDEF_H 1
#define PJ_HAS_STDIO_H 1
#define PJ_HAS_STDINT_H 1
#define PJ_HAS_STDLIB_H 1
#define PJ_HAS_STRING_H 1
#define PJ_HAS_SYS_IOCTL_H 1
#define PJ_HAS_SYS_SELECT_H 1
#define PJ_HAS_SYS_SOCKET_H 1
#define PJ_HAS_SYS_TIME_H 1
#define PJ_HAS_SYS_TIMEB_H 1
#define PJ_HAS_SYS_TYPES_H 1
/* #undef PJ_HAS_SYS_FILIO_H */
/* #undef PJ_HAS_SYS_SOCKIO_H */
#define PJ_HAS_SYS_UTSNAME_H 1
#define PJ_HAS_TIME_H 1
#define PJ_HAS_UNISTD_H 1

/* #undef PJ_HAS_MSWSOCK_H */
/* #undef PJ_HAS_WINSOCK_H */
/* #undef PJ_HAS_WINSOCK2_H */
/* #undef PJ_HAS_WS2TCPIP_H */

#define PJ_SOCK_HAS_INET_ATON 1
#define PJ_SOCK_HAS_INET_PTON 1
#define PJ_SOCK_HAS_INET_NTOP 1
#define PJ_SOCK_HAS_GETADDRINFO 1

/* On these OSes, semaphore feature depends on semaphore.h */
#if defined(PJ_HAS_SEMAPHORE_H) && PJ_HAS_SEMAPHORE_H!=0
#   define PJ_HAS_SEMAPHORE	1
#elif defined(PJ_WIN32) && PJ_WIN32!=0
#   define PJ_HAS_SEMAPHORE	1
#else
#   define PJ_HAS_SEMAPHORE	0
#endif

/* Do we have pthread_mutexattr_settype()? */
/* #undef PJ_HAS_PTHREAD_MUTEXATTR_SETTYPE */

/* Does pthread_mutexattr_t has "recursive" member?  */
/* #undef PJ_PTHREAD_MUTEXATTR_T_HAS_RECURSIVE */

/* Set 1 if native sockaddr_in has sin_len member. 
 * Default: 0
 */
/* #undef PJ_SOCKADDR_HAS_LEN */

/* Does the OS have socklen_t? */
#define PJ_HAS_SOCKLEN_T 1

#if !defined(socklen_t) && (!defined(PJ_HAS_SOCKLEN_T) || PJ_HAS_SOCKLEN_T==0)
# define PJ_HAS_SOCKLEN_T 1
  typedef int socklen_t;
#endif

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
#define PJ_SELECT_NEEDS_NFDS 0

/* Is errno a good way to retrieve OS errors?
 */
#define PJ_HAS_ERRNO_VAR 1

/* When this macro is set, getsockopt(SOL_SOCKET, SO_ERROR) will return
 * the status of non-blocking connect() operation.
 */
#define PJ_HAS_SO_ERROR 1

/* This value specifies the value set in errno by the OS when a non-blocking
 * socket recv() can not return immediate daata.
 */
#define PJ_BLOCKING_ERROR_VAL EAGAIN

/* This value specifies the value set in errno by the OS when a non-blocking
 * socket connect() can not get connected immediately.
 */
#define PJ_BLOCKING_CONNECT_ERROR_VAL EINPROGRESS

/* Default threading is enabled, unless it's overridden. */
#ifndef PJ_HAS_THREADS
#  define PJ_HAS_THREADS	    (1)
#endif

/* Do we need high resolution timer? */
#define PJ_HAS_HIGH_RES_TIMER 1

/* Is malloc() available? */
#define PJ_HAS_MALLOC 1

#ifndef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK    0
#endif

/* Unicode? */
#define PJ_NATIVE_STRING_IS_UNICODE 0

/* Pool alignment in bytes */
#define PJ_POOL_ALIGNMENT 4

/* The type of atomic variable value: */
#define PJ_ATOMIC_VALUE_TYPE long

#if defined(PJ_DARWINOS) && PJ_DARWINOS!=0
#    include "TargetConditionals.h"
#    if TARGET_OS_IPHONE
#	include "Availability.h"
	/* Use CFHost API for pj_getaddrinfo() (see ticket #1246) */
#	define PJ_GETADDRINFO_USE_CFHOST 1
	/* Disable local host resolution in pj_gethostip() (see ticket #1342) */
#	define PJ_GETHOSTIP_DISABLE_LOCAL_RESOLUTION 1
#    	ifdef __IPHONE_4_0
 	    /* Is multitasking support available?  (see ticket #1107) */
#	    define PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT 	1
	    /* Enable activesock TCP background mode support */
#	    define PJ_ACTIVESOCK_TCP_IPHONE_OS_BG		1
#	endif
#    endif
#endif

/* If 1, use Read/Write mutex emulation for platforms that don't support it */
#define PJ_EMULATE_RWMUTEX 0

/* If 1, pj_thread_create() should enforce the stack size when creating 
 * threads.
 * Default: 0 (let OS decide the thread's stack size).
 */
#define PJ_THREAD_SET_STACK_SIZE 0

/* If 1, pj_thread_create() should allocate stack from the pool supplied.
 * Default: 0 (let OS allocate memory for thread's stack).
 */
#define PJ_THREAD_ALLOCATE_STACK 0

/* SSL socket availability. */
#ifndef PJ_HAS_SSL_SOCK
#define PJ_HAS_SSL_SOCK 1
#endif


#endif	/* __PJ_COMPAT_OS_AUTO_H__ */


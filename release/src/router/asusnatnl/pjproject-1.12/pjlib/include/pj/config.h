/* $Id: config.h 3884 2011-11-08 17:11:29Z bennylp $ */
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
#ifndef __PJ_CONFIG_H__
#define __PJ_CONFIG_H__


/**
 * @file config.h
 * @brief PJLIB Main configuration settings.
 */

/********************************************************************
 * Include compiler specific configuration.
 */
#if defined(_MSC_VER)
#  include <pj/compat/cc_msvc.h>
#elif defined(__GNUC__)
#  include <pj/compat/cc_gcc.h>
#elif defined(__CW32__)
#  include <pj/compat/cc_mwcc.h>
#elif defined(__MWERKS__)
#  include <pj/compat/cc_codew.h>
#elif defined(__GCCE__)
#  include <pj/compat/cc_gcce.h>
#elif defined(__ARMCC__)
#  include <pj/compat/cc_armcc.h>
#else
#  error "Unknown compiler."
#endif


/********************************************************************
 * Include target OS specific configuration.
 */
#if defined(PJ_AUTOCONF)
    /*
     * Autoconf
     */
#   include <pj/compat/os_auto.h>

#elif defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
    /*
     * SymbianOS
     */
#  include <pj/compat/os_symbian.h>

#elif defined(PJ_WIN32_WINCE) || defined(_WIN32_WCE) || defined(UNDER_CE)
    /*
     * Windows CE
     */
#   undef PJ_WIN32_WINCE
#   define PJ_WIN32_WINCE   1
#   include <pj/compat/os_win32_wince.h>

    /* Also define Win32 */
#   define PJ_WIN32 1

#elif defined(PJ_WIN32_WINPHONE8) || defined(_WIN32_WINPHONE8)
    /*
     * Windows Phone 8
     */
#   undef PJ_WIN32_WINPHONE8
#   define PJ_WIN32_WINPHONE8   1
#   include <pj/compat/os_winphone8.h>

    /* Also define Win32 */
#   define PJ_WIN32 1

#elif defined(PJ_WIN32_UWP) || defined(_WIN32_UWP)
    /*
     * Windows UWP
     */
#   undef PJ_WIN32_UWP
#   define PJ_WIN32_UWP   1
#   include <pj/compat/os_winuwp.h>

    /* Define Windows phone */
#   define PJ_WIN32_WINPHONE8 1

    /* Also define Win32 */
#   define PJ_WIN32 1

#   if defined(PJ_WIN64) || defined(_WIN64) || defined(WIN64)
	/*
	* Win64
	*/
#	undef PJ_WIN64
#	define PJ_WIN64 1
#   endif

#elif defined(PJ_WIN32) || defined(_WIN32) || defined(__WIN32__) || \
	defined(WIN32) || defined(PJ_WIN64) || defined(_WIN64) || \
	defined(WIN64) || defined(__TOS_WIN__) 
#   if defined(PJ_WIN64) || defined(_WIN64) || defined(WIN64)
	/*
	 * Win64
	 */
#	undef PJ_WIN64
#	define PJ_WIN64 1
#   endif
#   undef PJ_WIN32
#   define PJ_WIN32 1
#   include <pj/compat/os_win32.h>

#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL!=0
    /*
     * Linux kernel
     */
#  include <pj/compat/os_linux_kernel.h>

#elif defined(PJ_LINUX) || defined(linux) || defined(__linux)
    /*
     * Linux
     */
#   undef PJ_LINUX
#   define PJ_LINUX	    1
#   include <pj/compat/os_linux.h>

#elif defined(PJ_PALMOS) && PJ_PALMOS!=0
    /*
     * Palm
     */
#  include <pj/compat/os_palmos.h>

#elif defined(PJ_SUNOS) || defined(sun) || defined(__sun)
    /*
     * SunOS
     */
#   undef PJ_SUNOS
#   define PJ_SUNOS	    1
#   include <pj/compat/os_sunos.h>

#elif defined(PJ_DARWINOS) || defined(__MACOSX__) || \
      defined (__APPLE__) || defined (__MACH__)
    /*
     * MacOS X
     */
#   undef PJ_DARWINOS
#   define PJ_DARWINOS	    1
#   include <pj/compat/os_darwinos.h>

#elif defined(PJ_RTEMS) && PJ_RTEMS!=0
    /*
     * RTEMS
     */
#  include <pj/compat/os_rtems.h>

#elif defined(PJ_ANDROID )
	/*
	 *	Android
	 */
#	include <pj/compat/os_android.h>
#else
#   error "Please specify target os."
#endif


/********************************************************************
 * Target machine specific configuration.
 */
#if defined(PJ_AUTOCONF)
    /*
     * Autoconf configured
     */
#include <pj/compat/m_auto.h>

#elif defined (PJ_M_I386) || defined(_i386_) || defined(i_386_) || \
	defined(_X86_) || defined(x86) || defined(__i386__) || \
	defined(__i386) || defined(_M_IX86) || defined(__I86__)
    /*
     * Generic i386 processor family, little-endian
     */
#   undef PJ_M_I386
#   define PJ_M_I386		1
#   define PJ_M_NAME		"i386"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0


#elif defined (PJ_M_X86_64) || defined(__amd64__) || defined(__amd64) || \
	defined(__x86_64__) || defined(__x86_64) || \
	defined(_M_X64) || defined(_M_AMD64)
    /*
     * AMD 64bit processor, little endian
     */
#   undef PJ_M_X86_64
#   define PJ_M_X86_64		1
#   define PJ_M_NAME		"x86_64"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0

#elif defined(PJ_M_IA64) || defined(__ia64__) || defined(_IA64) || \
	defined(__IA64__) || defined( 	_M_IA64)
    /*
     * Intel IA64 processor, default to little endian
     */
#   undef PJ_M_IA64
#   define PJ_M_IA64		1
#   define PJ_M_NAME		"ia64"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0

#elif defined (PJ_M_M68K) && PJ_M_M68K != 0

    /*
     * Motorola m68k processor, big endian
     */
#   undef PJ_M_M68K
#   define PJ_M_M68K		1
#   define PJ_M_NAME		"m68k"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	0
#   define PJ_IS_BIG_ENDIAN	1


#elif defined (PJ_M_ARM) || defined (__arm__) || defined (__arm) || \
	defined (_M_ARM)
    /*
     * DEC Alpha processor, little endian
     */
#   undef PJ_M_ARM
#   define PJ_M_ARM		1
#   define PJ_M_NAME		"arm"
#   define PJ_HAS_PENTIUM	0
#   if !PJ_IS_LITTLE_ENDIAN && !PJ_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif


#elif defined (PJ_M_ALPHA) || defined (__alpha__) || defined (__alpha) || \
	defined (_M_ALPHA)
    /*
     * DEC Alpha processor, little endian
     */
#   undef PJ_M_ALPHA
#   define PJ_M_ALPHA		1
#   define PJ_M_NAME		"alpha"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0


#elif defined(PJ_M_MIPS) || defined(__mips__) || defined(__mips) || \
	defined(__MIPS__) || defined(MIPS) || defined(_MIPS_)
    /*
     * MIPS, bi-endian, so raise error if endianness is not configured
     */
#   undef PJ_M_MIPS
#   define PJ_M_MIPS		1
#   define PJ_M_NAME		"mips"
#   define PJ_HAS_PENTIUM	0
#   if !PJ_IS_LITTLE_ENDIAN && !PJ_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif


#elif defined (PJ_M_SPARC) || defined( 	__sparc__) || defined(__sparc)
    /*
     * Sun Sparc, big endian
     */
#   undef PJ_M_SPARC
#   define PJ_M_SPARC		1
#   define PJ_M_NAME		"sparc"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	0
#   define PJ_IS_BIG_ENDIAN	1

#elif defined (PJ_M_ARMV4) || defined(ARM) || defined(_ARM_) ||  \
	defined(ARMV4) || defined(__arm__)
    /*
     * ARM, bi-endian, so raise error if endianness is not configured
     */
#   undef PJ_M_ARMV4
#   define PJ_M_ARMV4		1
#   define PJ_M_NAME		"armv4"
#   define PJ_HAS_PENTIUM	0
#   if !PJ_IS_LITTLE_ENDIAN && !PJ_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif

#elif defined (PJ_M_POWERPC) || defined(__powerpc) || defined(__powerpc__) || \
	defined(__POWERPC__) || defined(__ppc__) || defined(_M_PPC) || \
	defined(_ARCH_PPC)
    /*
     * PowerPC, bi-endian, so raise error if endianness is not configured
     */
#   undef PJ_M_POWERPC
#   define PJ_M_POWERPC		1
#   define PJ_M_NAME		"powerpc"
#   define PJ_HAS_PENTIUM	0
#   if !PJ_IS_LITTLE_ENDIAN && !PJ_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif

#elif defined (PJ_M_NIOS2) || defined(__nios2) || defined(__nios2__) || \
      defined(__NIOS2__) || defined(__M_NIOS2) || defined(_ARCH_NIOS2)
    /*
     * Nios2, little endian
     */
#   undef PJ_M_NIOS2
#   define PJ_M_NIOS2		1
#   define PJ_M_NAME		"nios2"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0
		
#else
#   error "Please specify target machine."
#endif

/* Include size_t definition. */
#include <pj/compat/size_t.h>

/* Include site/user specific configuration to control PJLIB features.
 * YOU MUST CREATE THIS FILE YOURSELF!!
 */
#include <pj/config_site.h>

/********************************************************************
 * PJLIB Features.
 */

/* Overrides for DOXYGEN */
#ifdef DOXYGEN
#   undef PJ_FUNCTIONS_ARE_INLINED
#   undef PJ_HAS_FLOATING_POINT
#   undef PJ_LOG_MAX_LEVEL
#   undef PJ_LOG_MAX_SIZE
#   undef PJ_LOG_USE_STACK_BUFFER
#   undef PJ_TERM_HAS_COLOR
#   undef PJ_POOL_DEBUG
#   undef PJ_HAS_TCP
#   undef PJ_MAX_HOSTNAME
#   undef PJ_IOQUEUE_MAX_HANDLES
#   undef FD_SETSIZE
#   undef PJ_HAS_SEMAPHORE
#   undef PJ_HAS_EVENT_OBJ
#   undef PJ_ENABLE_EXTRA_CHECK
#   undef PJ_EXCEPTION_USE_WIN32_SEH
#   undef PJ_HAS_ERROR_STRING

#   define PJ_HAS_IPV6	1
#endif

/**
 * @defgroup pj_config Build Configuration
 * @{
 *
 * This section contains macros that can set during PJLIB build process
 * to controll various aspects of the library.
 *
 * <b>Note</b>: the values in this page does NOT necessarily reflect to the
 * macro values during the build process.
 */

/**
 * If this macro is set to 1, it will enable some debugging checking
 * in the library.
 *
 * Default: equal to (NOT NDEBUG).
 */
#ifndef PJ_DEBUG
#  ifndef NDEBUG
#    define PJ_DEBUG		    1
#  else
#    define PJ_DEBUG		    0
#  endif
#endif

/**
 * Enable this macro to activate logging to mutex/semaphore related events.
 * This is useful to troubleshoot concurrency problems such as deadlocks.
 * In addition, you should also add PJ_LOG_HAS_THREAD_ID flag to the
 * log decoration to assist the troubleshooting.
 *
 * Default: 0
 */
#ifndef PJ_DEBUG_MUTEX
#   define PJ_DEBUG_MUTEX	    0
#endif

/**
 * Expand functions in *_i.h header files as inline.
 *
 * Default: 0.
 */
#ifndef PJ_FUNCTIONS_ARE_INLINED
#  define PJ_FUNCTIONS_ARE_INLINED  0
#endif

/**
 * Use floating point computations in the library.
 *
 * Default: 1.
 */
#ifndef PJ_HAS_FLOATING_POINT
#  define PJ_HAS_FLOATING_POINT	    1
#endif

/**
 * Declare maximum logging level/verbosity. Lower number indicates higher
 * importance, with the highest importance has level zero. The least
 * important level is five in this implementation, but this can be extended
 * by supplying the appropriate implementation.
 *
 * The level conventions:
 *  - 0: fatal error
 *  - 1: error
 *  - 2: warning
 *  - 3: info
 *  - 4: debug
 *  - 5: trace
 *  - 6: more detailed trace
 *
 * Default: 4
 */
#ifndef PJ_LOG_MAX_LEVEL
#  define PJ_LOG_MAX_LEVEL   5
#endif

/**
 * Maximum message size that can be sent to output device for each call
 * to PJ_LOG(). If the message size is longer than this value, it will be cut.
 * This may affect the stack usage, depending whether PJ_LOG_USE_STACK_BUFFER
 * flag is set.
 *
 * Default: 4000
 */
#ifndef PJ_LOG_MAX_SIZE
#  define PJ_LOG_MAX_SIZE	    6000
#endif

/**
 * Log buffer.
 * Does the log get the buffer from the stack? (default is yes).
 * If the value is set to NO, then the buffer will be taken from static
 * buffer, which in this case will make the log function non-reentrant.
 *
 * Default: 1
 */
#ifndef PJ_LOG_USE_STACK_BUFFER
#  define PJ_LOG_USE_STACK_BUFFER   1
#endif


/**
 * Colorfull terminal (for logging etc).
 *
 * Default: 1
 */
#ifndef PJ_TERM_HAS_COLOR
#  define PJ_TERM_HAS_COLOR	    1
#endif


/**
 * Set this flag to non-zero to enable various checking for pool
 * operations. When this flag is set, assertion must be enabled
 * in the application.
 *
 * This will slow down pool creation and destruction and will add
 * few bytes of overhead, so application would normally want to 
 * disable this feature on release build.
 *
 * Default: 0
 */
#ifndef PJ_SAFE_POOL
#   define PJ_SAFE_POOL		    0
#endif


/**
 * If pool debugging is used, then each memory allocation from the pool
 * will call malloc(), and pool will release all memory chunks when it
 * is destroyed. This works better when memory verification programs
 * such as Rational Purify is used.
 *
 * Default: 0
 */
#ifndef PJ_POOL_DEBUG
#  define PJ_POOL_DEBUG		    0
#endif


/**
 * Enable timer heap debugging facility. When this is enabled, application
 * can call pj_timer_heap_dump() to show the contents of the timer heap
 * along with the source location where the timer entries were scheduled.
 * See https://trac.pjsip.org/repos/ticket/1527 for more info.
 *
 * Default: 0
 */
#ifndef PJ_TIMER_DEBUG
#  define PJ_TIMER_DEBUG	    0
#endif


/**
 * Set this to 1 to enable debugging on the group lock. Default: 0
 */
#ifndef PJ_GRP_LOCK_DEBUG
#  define PJ_GRP_LOCK_DEBUG	0
#endif


/**
 * Specify this as \a stack_size argument in #pj_thread_create() to specify
 * that thread should use default stack size for the current platform.
 *
 * Default: 8192
 */
#ifndef PJ_THREAD_DEFAULT_STACK_SIZE 
#  define PJ_THREAD_DEFAULT_STACK_SIZE    8192
#endif


/**
 * Specify if PJ_CHECK_STACK() macro is enabled to check the sanity of 
 * the stack. The OS implementation may check that no stack overflow 
 * occurs, and it also may collect statistic about stack usage. Note
 * that this will increase the footprint of the libraries since it
 * tracks the filename and line number of each functions.
 */
#ifndef PJ_OS_HAS_CHECK_STACK
#	define PJ_OS_HAS_CHECK_STACK		0
#endif

/**
 * Do we have alternate pool implementation?
 *
 * Default: 0
 */
#ifndef PJ_HAS_POOL_ALT_API
#   define PJ_HAS_POOL_ALT_API	    PJ_POOL_DEBUG
#endif


/**
 * Support TCP in the library.
 * Disabling TCP will reduce the footprint slightly (about 6KB).
 *
 * Default: 1
 */
#ifndef PJ_HAS_TCP
#  define PJ_HAS_TCP		    1
#endif

/**
 * Support IPv6 in the library. If this support is disabled, some IPv6 
 * related functions will return PJ_EIPV6NOTSUP.
 *
 * Default: 0 (disabled, for now)
 */
#ifndef PJ_HAS_IPV6
#  define PJ_HAS_IPV6		    0
#endif

 /**
 * Maximum hostname length.
 * Libraries sometimes needs to make copy of an address to stack buffer;
 * the value here affects the stack usage.
 *
 * Default: 128
 */
#ifndef PJ_MAX_HOSTNAME
#  define PJ_MAX_HOSTNAME	    (128)
#endif

/**
 * Maximum consecutive identical error for accept() operation before
 * activesock stops calling the next ioqueue accept.
 *
 * Default: 50
 */
#ifndef PJ_ACTIVESOCK_MAX_CONSECUTIVE_ACCEPT_ERROR
#   define PJ_ACTIVESOCK_MAX_CONSECUTIVE_ACCEPT_ERROR 50
#endif

/**
 * Constants for declaring the maximum handles that can be supported by
 * a single IOQ framework. This constant might not be relevant to the 
 * underlying I/O queue impelementation, but still, developers should be 
 * aware of this constant, to make sure that the program will not break when
 * the underlying implementation changes.
 */
#ifndef PJ_IOQUEUE_MAX_HANDLES
#   define PJ_IOQUEUE_MAX_HANDLES	(64)
#endif


/**
 * If PJ_IOQUEUE_HAS_SAFE_UNREG macro is defined, then ioqueue will do more
 * things to ensure thread safety of handle unregistration operation by
 * employing reference counter to each handle.
 *
 * In addition, the ioqueue will preallocate memory for the handles, 
 * according to the maximum number of handles that is specified during 
 * ioqueue creation.
 *
 * All applications would normally want this enabled, but you may disable
 * this if:
 *  - there is no dynamic unregistration to all ioqueues.
 *  - there is no threading, or there is no preemptive multitasking.
 *
 * Default: 1
 */
#ifndef PJ_IOQUEUE_HAS_SAFE_UNREG
#   define PJ_IOQUEUE_HAS_SAFE_UNREG	1
#endif


/**
 * Default concurrency setting for sockets/handles registered to ioqueue.
 * This controls whether the ioqueue is allowed to call the key's callback
 * concurrently/in parallel. The default is yes, which means that if there
 * are more than one pending operations complete simultaneously, more
 * than one threads may call the key's callback at the same time. This
 * generally would promote good scalability for application, at the 
 * expense of more complexity to manage the concurrent accesses.
 *
 * Please see the ioqueue documentation for more info.
 */
#ifndef PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY
#   define PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY   1
#endif


/* Sanity check:
 *  if ioqueue concurrency is disallowed, PJ_IOQUEUE_HAS_SAFE_UNREG
 *  must be enabled.
 */
#if (PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY==0) && (PJ_IOQUEUE_HAS_SAFE_UNREG==0)
#   error PJ_IOQUEUE_HAS_SAFE_UNREG must be enabled if ioqueue concurrency \
	  is disabled
#endif


/**
 * When safe unregistration (PJ_IOQUEUE_HAS_SAFE_UNREG) is configured in
 * ioqueue, the PJ_IOQUEUE_KEY_FREE_DELAY macro specifies how long the
 * ioqueue key is kept in closing state before it can be reused.
 *
 * The value is in miliseconds.
 *
 * Default: 500 msec.
 */
#ifndef PJ_IOQUEUE_KEY_FREE_DELAY
#   define PJ_IOQUEUE_KEY_FREE_DELAY	500
#endif


/**
 * Determine if FD_SETSIZE is changeable/set-able. If so, then we will
 * set it to PJ_IOQUEUE_MAX_HANDLES. Currently we detect this by checking
 * for Winsock.
 */
#ifndef PJ_FD_SETSIZE_SETABLE
#   if (defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H!=0) || \
       (defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H!=0)
#	define PJ_FD_SETSIZE_SETABLE	1
#   else
#	define PJ_FD_SETSIZE_SETABLE	0
#   endif
#endif

/**
 * Overrides FD_SETSIZE so it is consistent throughout the library.
 * We only do this if we detected that FD_SETSIZE is changeable. If
 * FD_SETSIZE is not set-able, then PJ_IOQUEUE_MAX_HANDLES must be
 * set to value lower than FD_SETSIZE.
 */
#if PJ_FD_SETSIZE_SETABLE
    /* Only override FD_SETSIZE if the value has not been set */
#   ifndef FD_SETSIZE
#	define FD_SETSIZE		PJ_IOQUEUE_MAX_HANDLES
#   endif
#else
    /* When FD_SETSIZE is not changeable, check if PJ_IOQUEUE_MAX_HANDLES
     * is lower than FD_SETSIZE value.
     */
#   ifdef FD_SETSIZE
#	if PJ_IOQUEUE_MAX_HANDLES > FD_SETSIZE
#	    error "PJ_IOQUEUE_MAX_HANDLES is greater than FD_SETSIZE"
#	endif
#   endif
#endif


/**
 * Specify whether #pj_enum_ip_interface() function should exclude
 * loopback interfaces.
 *
 * Default: 1
 */
#ifndef PJ_IP_HELPER_IGNORE_LOOPBACK_IF
#   define PJ_IP_HELPER_IGNORE_LOOPBACK_IF	1
#endif


/**
 * Specify whether #pj_get_physical_address_by_ip() function should exclude
 * point-to-point interfaces.
 *
 * Default: 1
 */
#ifndef PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF
#   define PJ_IP_HELPER_IGNORE_POINT_TO_POINT_IF	1
#endif


/**
 * Has semaphore functionality?
 *
 * Default: 1
 */
#ifndef PJ_HAS_SEMAPHORE
#  define PJ_HAS_SEMAPHORE	    1
#endif


/**
 * Event object (for synchronization, e.g. in Win32)
 *
 * Default: 1
 */
#ifndef PJ_HAS_EVENT_OBJ
#  define PJ_HAS_EVENT_OBJ	    1
#endif


/**
 * Maximum file name length.
 */
#ifndef PJ_MAXPATH
#   define PJ_MAXPATH		    260
#endif


/**
 * Enable library's extra check.
 * If this macro is enabled, #PJ_ASSERT_RETURN macro will expand to
 * run-time checking. If this macro is disabled, #PJ_ASSERT_RETURN
 * will simply evaluate to #pj_assert().
 *
 * You can disable this macro to reduce size, at the risk of crashes
 * if invalid value (e.g. NULL) is passed to the library.
 *
 * Default: 1
 */
#ifndef PJ_ENABLE_EXTRA_CHECK
#   define PJ_ENABLE_EXTRA_CHECK    1
#endif


/**
 * Enable name registration for exceptions with #pj_exception_id_alloc().
 * If this feature is enabled, then the library will keep track of
 * names associated with each exception ID requested by application via
 * #pj_exception_id_alloc().
 *
 * Disabling this macro will reduce the code and .bss size by a tad bit.
 * See also #PJ_MAX_EXCEPTION_ID.
 *
 * Default: 1
 */
#ifndef PJ_HAS_EXCEPTION_NAMES
#   define PJ_HAS_EXCEPTION_NAMES   1
#endif

/**
 * Maximum number of unique exception IDs that can be requested
 * with #pj_exception_id_alloc(). For each entry, a small record will
 * be allocated in the .bss segment.
 *
 * Default: 16
 */
#ifndef PJ_MAX_EXCEPTION_ID
#   define PJ_MAX_EXCEPTION_ID      16
#endif

/**
 * Should we use Windows Structured Exception Handling (SEH) for the
 * PJLIB exceptions.
 *
 * Default: 0
 */
#ifndef PJ_EXCEPTION_USE_WIN32_SEH
#  define PJ_EXCEPTION_USE_WIN32_SEH 0
#endif

/**
 * Should we attempt to use Pentium's rdtsc for high resolution
 * timestamp.
 *
 * Default: 0
 */
#ifndef PJ_TIMESTAMP_USE_RDTSC
#   define PJ_TIMESTAMP_USE_RDTSC   0
#endif

/**
 * Is native platform error positive number?
 * Default: 1 (yes)
 */
#ifndef PJ_NATIVE_ERR_POSITIVE
#   define PJ_NATIVE_ERR_POSITIVE   1
#endif
 
/**
 * Include error message string in the library (pj_strerror()).
 * This is very much desirable!
 *
 * Default: 1
 */
#ifndef PJ_HAS_ERROR_STRING
#   define PJ_HAS_ERROR_STRING	    1
#endif


/**
 * Include pj_stricmp_alnum() and pj_strnicmp_alnum(), i.e. custom
 * functions to compare alnum strings. On some systems, they're faster
 * then stricmp/strcasecmp, but they can be slower on other systems.
 * When disabled, pjlib will fallback to stricmp/strnicmp.
 * 
 * Default: 0
 */
#ifndef PJ_HAS_STRICMP_ALNUM
#   define PJ_HAS_STRICMP_ALNUM	    0
#endif


/*
 * Types of QoS backend implementation.
 */

/** 
 * Dummy QoS backend implementation, will always return error on all
 * the APIs.
 */
#define PJ_QOS_DUMMY	    1

/** QoS backend based on setsockopt(IP_TOS) */
#define PJ_QOS_BSD	    2

/** QoS backend for Windows Mobile 6 */
#define PJ_QOS_WM	    3

/** QoS backend for Symbian */
#define PJ_QOS_SYMBIAN	    4

/**
 * Force the use of some QoS backend API for some platforms.
 */
#ifndef PJ_QOS_IMPLEMENTATION
#   if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE && _WIN32_WCE >= 0x502
	/* Windows Mobile 6 or later */
#	define PJ_QOS_IMPLEMENTATION    PJ_QOS_WM
#   endif
#endif


/**
 * Enable secure socket. For most platforms, this is implemented using
 * OpenSSL, so this will require OpenSSL to be installed. For Symbian
 * platform, this is implemented natively using CSecureSocket.
 *
 * Default: 0 (for now)
 */
#ifndef PJ_HAS_SSL_SOCK
#  define PJ_HAS_SSL_SOCK	    0
#endif


/**
 * Disable WSAECONNRESET error for UDP sockets on Win32 platforms. See
 * https://trac.pjsip.org/repos/ticket/1197.
 *
 * Default: 1
 */
#ifndef PJ_SOCK_DISABLE_WSAECONNRESET
#   define PJ_SOCK_DISABLE_WSAECONNRESET    1
#endif


/** @} */

/********************************************************************
 * General macros.
 */

/**
 * @defgroup pj_dll_target Building Dynamic Link Libraries (DLL/DSO)
 * @ingroup pj_config
 * @{
 *
 * The libraries support generation of dynamic link libraries for
 * Symbian ABIv2 target (.dso/Dynamic Shared Object files, in Symbian
 * terms). Similar procedures may be applied for Win32 DLL with some 
 * modification.
 *
 * Depending on the platforms, these steps may be necessary in order to
 * produce the dynamic libraries:
 *  - Create the (Visual Studio) projects to produce DLL output. PJLIB
 *    does not provide ready to use project files to produce DLL, so
 *    you need to create these projects yourself. For Symbian, the MMP
 *    files have been setup to produce DSO files for targets that 
 *    require them.
 *  - In the (Visual Studio) projects, some macros need to be declared
 *    so that appropriate modifiers are added to symbol declarations
 *    and definitions. Please see the macro section below for information
 *    regarding these macros. For Symbian, these have been taken care by the
 *    MMP files.
 *  - Some build systems require .DEF file to be specified when creating
 *    the DLL. For Symbian, .DEF files are included in pjlib distribution,
 *    in <tt>pjlib/build.symbian</tt> directory. These DEF files are 
 *    created by running <tt>./makedef.sh all</tt> from this directory,
 *    inside Mingw.
 *
 * Macros related for building DLL/DSO files:
 *  - For platforms that supports dynamic link libraries generation,
 *    it must declare <tt>PJ_EXPORT_SPECIFIER</tt> macro which value contains
 *    the prefix to be added to symbol definition, to export this 
 *    symbol in the DLL/DSO. For example, on Win32/Visual Studio, the
 *    value of this macro is \a __declspec(dllexport), and for ARM 
 *    ABIv2/Symbian, the value is \a EXPORT_C. 
 *  - For platforms that supports linking with dynamic link libraries,
 *    it must declare <tt>PJ_IMPORT_SPECIFIER</tt> macro which value contains
 *    the prefix to be added to symbol declaration, to import this 
 *    symbol from a DLL/DSO. For example, on Win32/Visual Studio, the
 *    value of this macro is \a __declspec(dllimport), and for ARM 
 *    ABIv2/Symbian, the value is \a IMPORT_C. 
 *  - Both <tt>PJ_EXPORT_SPECIFIER</tt> and <tt>PJ_IMPORT_SPECIFIER</tt> 
 *    macros above can be declared in your \a config_site.h if they are not
 *    declared by pjlib.
 *  - When PJLIB is built as DLL/DSO, both <tt>PJ_DLL</tt> and 
 *    <tt>PJ_EXPORTING</tt> macros must be declared, so that 
 *     <tt>PJ_EXPORT_SPECIFIER</tt> modifier will be added into function
 *    definition.
 *  - When application wants to link dynamically with PJLIB, then it
 *    must declare <tt>PJ_DLL</tt> macro when using/including PJLIB header,
 *    so that <tt>PJ_IMPORT_SPECIFIER</tt> modifier is properly added into 
 *    symbol declarations.
 *
 * When <b>PJ_DLL</b> macro is not declared, static linking is assumed.
 *
 * For example, here are some settings to produce DLLs with Visual Studio
 * on Windows/Win32:
 *  - Create Visual Studio projects to produce DLL. Add the appropriate 
 *    project dependencies to avoid link errors.
 *  - In the projects, declare <tt>PJ_DLL</tt> and <tt>PJ_EXPORTING</tt> 
 *    macros.
 *  - Declare these macros in your <tt>config_site.h</tt>:
 \verbatim
	#define PJ_EXPORT_SPECIFIER  __declspec(dllexport)
	#define PJ_IMPORT_SPECIFIER  __declspec(dllimport)
 \endverbatim
 *  - And in the application (that links with the DLL) project, add 
 *    <tt>PJ_DLL</tt> in the macro declarations.
 */

/** @} */

/**
 * @defgroup pj_config Build Configuration
 * @{
 */

/**
 * @def PJ_INLINE(type)
 * @param type The return type of the function.
 * Expand the function as inline.
 */
#define PJ_INLINE(type)	  PJ_INLINE_SPECIFIER type

/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol declaration to export the symbol when PJLIB
 * is built as dynamic library.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target. 
 */
#ifndef PJ_EXPORT_DECL_SPECIFIER
#   define PJ_EXPORT_DECL_SPECIFIER
#endif


/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol definition to export the symbol when PJLIB
 * is built as dynamic library.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target. 
 */
#ifndef PJ_EXPORT_DEF_SPECIFIER
#   define PJ_EXPORT_DEF_SPECIFIER
#endif


/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol declaration to import the symbol.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target.
 */
#ifndef PJ_IMPORT_DECL_SPECIFIER
#   define PJ_IMPORT_DECL_SPECIFIER
#endif


/**
 * This macro has been deprecated. It will evaluate to nothing.
 */
#ifndef PJ_EXPORT_SYMBOL
#   define PJ_EXPORT_SYMBOL(x)
#endif


/**
 * @def PJ_DECL(type)
 * @param type The return type of the function.
 * Declare a function.
 */
#if defined(PJ_DLL)
#   if defined(PJ_EXPORTING)
#	define PJ_DECL(type)	    PJ_EXPORT_DECL_SPECIFIER type
#   else
#	define PJ_DECL(type)	    PJ_IMPORT_DECL_SPECIFIER type
#   endif
#elif !defined(PJ_DECL)
#   if defined(__cplusplus)
#	define PJ_DECL(type)	    type
#   else
#	define PJ_DECL(type)	    extern type
#   endif
#endif


/**
 * @def PJ_DEF(type)
 * @param type The return type of the function.
 * Define a function.
 */
#if defined(PJ_DLL) && defined(PJ_EXPORTING)
#   define PJ_DEF(type)		    PJ_EXPORT_DEF_SPECIFIER type
#elif !defined(PJ_DEF)
#   define PJ_DEF(type)		    type
#endif


/**
 * @def PJ_DECL_NO_RETURN(type)
 * @param type The return type of the function.
 * Declare a function that will not return.
 */
/**
 * @def PJ_IDECL_NO_RETURN(type)
 * @param type The return type of the function.
 * Declare an inline function that will not return.
 */
/**
 * @def PJ_BEGIN_DECL
 * Mark beginning of declaration section in a header file.
 */
/**
 * @def PJ_END_DECL
 * Mark end of declaration section in a header file.
 */
#ifdef __cplusplus
#  define PJ_DECL_NO_RETURN(type)   PJ_DECL(type) PJ_NORETURN
#  define PJ_IDECL_NO_RETURN(type)  PJ_INLINE(type) PJ_NORETURN
#  define PJ_BEGIN_DECL		    extern "C" {
#  define PJ_END_DECL		    }
#else
#  define PJ_DECL_NO_RETURN(type)   PJ_NORETURN PJ_DECL(type)
#  define PJ_IDECL_NO_RETURN(type)  PJ_NORETURN PJ_INLINE(type)
#  define PJ_BEGIN_DECL
#  define PJ_END_DECL
#endif



/**
 * @def PJ_DECL_DATA(type)
 * @param type The data type.
 * Declare a global data.
 */ 
#if defined(PJ_DLL)
#   if defined(PJ_EXPORTING)
#	define PJ_DECL_DATA(type)   PJ_EXPORT_DECL_SPECIFIER extern type
#   else
#	define PJ_DECL_DATA(type)   PJ_IMPORT_DECL_SPECIFIER extern type
#   endif
#elif !defined(PJ_DECL_DATA)
#   define PJ_DECL_DATA(type)	    extern type
#endif


/**
 * @def PJ_DEF_DATA(type)
 * @param type The data type.
 * Define a global data.
 */ 
#if defined(PJ_DLL) && defined(PJ_EXPORTING)
#   define PJ_DEF_DATA(type)	    PJ_EXPORT_DEF_SPECIFIER type
#elif !defined(PJ_DEF_DATA)
#   define PJ_DEF_DATA(type)	    type
#endif


/**
 * @def PJ_IDECL(type)
 * @param type  The function's return type.
 * Declare a function that may be expanded as inline.
 */
/**
 * @def PJ_IDEF(type)
 * @param type  The function's return type.
 * Define a function that may be expanded as inline.
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  define PJ_IDECL(type)  PJ_INLINE(type)
#  define PJ_IDEF(type)   PJ_INLINE(type)
#else
#  define PJ_IDECL(type)  PJ_DECL(type)
#  define PJ_IDEF(type)   PJ_DEF(type)
#endif


/**
 * @def PJ_UNUSED_ARG(arg)
 * @param arg   The argument name.
 * PJ_UNUSED_ARG prevents warning about unused argument in a function.
 */
#define PJ_UNUSED_ARG(arg)  (void)arg

/**
 * @def PJ_TODO(id)
 * @param id    Any identifier that will be printed as TODO message.
 * PJ_TODO macro will display TODO message as warning during compilation.
 * Example: PJ_TODO(CLEAN_UP_ERROR);
 */
#ifndef PJ_TODO
#  define PJ_TODO(id)	    TODO___##id:
#endif

/**
 * Simulate race condition by sleeping the thread in strategic locations.
 * Default: no!
 */
#ifndef PJ_RACE_ME
#  define PJ_RACE_ME(x)
#endif

/**
 * Function attributes to inform that the function may throw exception.
 *
 * @param x     The exception list, enclosed in parenthesis.
 */
#define __pj_throw__(x)

/** @} */

/********************************************************************
 * Sanity Checks
 */
#ifndef PJ_HAS_HIGH_RES_TIMER
#  error "PJ_HAS_HIGH_RES_TIMER is not defined!"
#endif

#if !defined(PJ_HAS_PENTIUM)
#  error "PJ_HAS_PENTIUM is not defined!"
#endif

#if !defined(PJ_IS_LITTLE_ENDIAN)
#  error "PJ_IS_LITTLE_ENDIAN is not defined!"
#endif

#if !defined(PJ_IS_BIG_ENDIAN)
#  error "PJ_IS_BIG_ENDIAN is not defined!"
#endif

#if !defined(PJ_EMULATE_RWMUTEX)
#  error "PJ_EMULATE_RWMUTEX should be defined in compat/os_xx.h"
#endif

#if !defined(PJ_THREAD_SET_STACK_SIZE)
#  error "PJ_THREAD_SET_STACK_SIZE should be defined in compat/os_xx.h"
#endif

#if !defined(PJ_THREAD_ALLOCATE_STACK)
#  error "PJ_THREAD_ALLOCATE_STACK should be defined in compat/os_xx.h"
#endif

PJ_BEGIN_DECL

/** PJLIB version major number. */
#define PJ_VERSION_NUM_MAJOR	1

/** PJLIB version minor number. */
#define PJ_VERSION_NUM_MINOR	12

/** PJLIB version revision number. */
#define PJ_VERSION_NUM_REV	0

/**
 * Extra suffix for the version (e.g. "-trunk"), or empty for
 * web release version.
 */
#define PJ_VERSION_NUM_EXTRA	""

/**
 * PJLIB version number consists of three bytes with the following format:
 * 0xMMIIRR00, where MM: major number, II: minor number, RR: revision
 * number, 00: always zero for now.
 */
#define PJ_VERSION_NUM	((PJ_VERSION_NUM_MAJOR << 24) |	\
			 (PJ_VERSION_NUM_MINOR << 16) | \
			 (PJ_VERSION_NUM_REV << 8))

/**
 * PJLIB version string constant. @see pj_get_version()
 */
PJ_DECL_DATA(const char*) PJ_VERSION;

/**
 * Get PJLIB version string.
 *
 * @return #PJ_VERSION constant.
 */
PJ_DECL(const char*) pj_get_version(void);

/**
 * Dump configuration to log with verbosity equal to info(3).
 */
PJ_DECL(void) pj_dump_config(void);

PJ_END_DECL


#endif	/* __PJ_CONFIG_H__ */


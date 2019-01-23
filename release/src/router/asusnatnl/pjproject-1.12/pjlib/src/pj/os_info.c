/* $Id: os_info.c 4427 2013-03-07 06:04:43Z nanang $ */
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
#include <pj/os.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/string.h>

/*
 * FYI these links contain useful infos about predefined macros across
 * platforms:
 *  - http://predef.sourceforge.net/preos.html
 */

#if defined(PJ_HAS_SYS_UTSNAME_H) && PJ_HAS_SYS_UTSNAME_H != 0
/* For uname() */
#   include <sys/utsname.h>
#   include <stdlib.h>
#   define PJ_HAS_UNAME		1
#endif

#if defined(PJ_HAS_LIMITS_H) && PJ_HAS_LIMITS_H != 0
/* Include <limits.h> to get <features.h> to get various glibc macros.
 * See http://predef.sourceforge.net/prelib.html
 */
#   include <limits.h>
#endif

#if defined(_MSC_VER)
/* For all Windows including mobile */
#   include <windows.h>
#endif

#if defined(PJ_DARWINOS) && PJ_DARWINOS != 0
#   include "TargetConditionals.h"
#endif

#ifndef PJ_SYS_INFO_BUFFER_SIZE
#   define PJ_SYS_INFO_BUFFER_SIZE	64
#endif


#if defined(PJ_DARWINOS) && PJ_DARWINOS != 0 && TARGET_OS_IPHONE
#   include <sys/types.h>
#   include <sys/sysctl.h>
    void pj_iphone_os_get_sys_info(pj_sys_info *si, pj_str_t *si_buffer);
#endif
    
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    PJ_BEGIN_DECL
    unsigned pj_symbianos_get_model_info(char *buf, unsigned buf_size);
    unsigned pj_symbianos_get_platform_info(char *buf, unsigned buf_size);
    void pj_symbianos_get_sdk_info(pj_str_t *name, pj_uint32_t *ver);
    PJ_END_DECL
#endif


static char *ver_info(pj_uint32_t ver, char *buf)
{
    pj_size_t len;

    if (ver == 0) {
	*buf = '\0';
	return buf;
    }

    sprintf(buf, "-%u.%u",
	    (ver & 0xFF000000) >> 24,
	    (ver & 0x00FF0000) >> 16);
    len = strlen(buf);

    if (ver & 0xFFFF) {
	sprintf(buf+len, ".%u", (ver & 0xFF00) >> 8);
	len = strlen(buf);

	if (ver & 0x00FF) {
	    sprintf(buf+len, ".%u", (ver & 0xFF));
	}
    }

    return buf;
}

static pj_uint32_t parse_version(char *str)
{
    char *tok;
    int i, maxtok;
    pj_uint32_t version = 0;
    
    while (*str && !pj_isdigit(*str))
	str++;

    maxtok = 4;
    for (tok = strtok(str, ".-"), i=0; tok && i<maxtok;
	 ++i, tok=strtok(NULL, ".-"))
    {
	int n;

	if (!pj_isdigit(*tok))
	    break;
	
	n = atoi(tok);
	version |= (n << ((3-i)*8));
    }
    
    return version;
}

PJ_DEF(const pj_sys_info*) pj_get_sys_info(void)
{
    static char si_buffer[PJ_SYS_INFO_BUFFER_SIZE];
    static pj_sys_info si;
    static pj_bool_t si_initialized;
    pj_size_t left = PJ_SYS_INFO_BUFFER_SIZE, len;

    if (si_initialized)
	return &si;

    si.machine.ptr = si.os_name.ptr = si.sdk_name.ptr = si.info.ptr = "";

#define ALLOC_CP_STR(str,field)	\
	do { \
	    len = pj_ansi_strlen(str); \
	    if (len && left >= len+1) { \
		si.field.ptr = si_buffer + PJ_SYS_INFO_BUFFER_SIZE - left; \
		si.field.slen = len; \
		pj_memcpy(si.field.ptr, str, len+1); \
		left -= (len+1); \
	    } \
	} while (0)

    /*
     * Machine and OS info.
     */
#if defined(PJ_HAS_UNAME) && PJ_HAS_UNAME
    #if defined(PJ_DARWINOS) && PJ_DARWINOS != 0 && TARGET_OS_IPHONE && \
	(!defined TARGET_IPHONE_SIMULATOR || TARGET_IPHONE_SIMULATOR == 0)
    {
	pj_str_t buf = {si_buffer + PJ_SYS_INFO_BUFFER_SIZE - left, left};
	pj_str_t machine = {"arm-", 4};
	pj_str_t sdk_name = {"iOS-SDK", 7};
        size_t size = PJ_SYS_INFO_BUFFER_SIZE - machine.slen;
	char tmp[PJ_SYS_INFO_BUFFER_SIZE];
        int name[] = {CTL_HW,HW_MACHINE};

	pj_iphone_os_get_sys_info(&si, &buf);
	left -= si.os_name.slen + 1;

	si.os_ver = parse_version(si.machine.ptr);

	pj_memcpy(tmp, machine.ptr, machine.slen);
        sysctl(name, 2, tmp+machine.slen, &size, NULL, 0);
        ALLOC_CP_STR(tmp, machine);
	si.sdk_name = sdk_name;

	#ifdef PJ_SDK_NAME
	pj_memcpy(tmp, PJ_SDK_NAME, pj_ansi_strlen(PJ_SDK_NAME) + 1);
	si.sdk_ver = parse_version(tmp);
	#endif
    }
    #else    
    {
	struct utsname u;

	/* Successful uname() returns zero on Linux and positive value
	 * on OpenSolaris.
	 */
	if (uname(&u) == -1)
	    goto get_sdk_info;

	ALLOC_CP_STR(u.machine, machine);
	ALLOC_CP_STR(u.sysname, os_name);
	
	si.os_ver = parse_version(u.release);
    }
    #endif
#elif defined(_MSC_VER)
    {
    #if defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8
	si.os_name = pj_str("winphone");
    #else
	OSVERSIONINFO ovi;

	ovi.dwOSVersionInfoSize = sizeof(ovi);

	if (GetVersionEx(&ovi) == FALSE)
	    goto get_sdk_info;

	si.os_ver = (ovi.dwMajorVersion << 24) |
		    (ovi.dwMinorVersion << 16);
	#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
	    si.os_name = pj_str("wince");
	#else
	    si.os_name = pj_str("win32");
	#endif
    #endif
    }

    {
	SYSTEM_INFO wsi;

    #if defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8
	GetNativeSystemInfo(&wsi);
    #else
	GetSystemInfo(&wsi);
    #endif
	
	switch (wsi.wProcessorArchitecture) {
	#if (defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE) || \
	    (defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8)
	case PROCESSOR_ARCHITECTURE_ARM:
	    si.machine = pj_str("arm");
	    break;
	case PROCESSOR_ARCHITECTURE_SHX:
	    si.machine = pj_str("shx");
	    break;
    #else
	case PROCESSOR_ARCHITECTURE_AMD64:
	    si.machine = pj_str("x86_64");
	    break;
	case PROCESSOR_ARCHITECTURE_IA64:
	    si.machine = pj_str("ia64");
	    break;
	case PROCESSOR_ARCHITECTURE_INTEL:
	    si.machine = pj_str("i386");
	    break;
    #endif	/* PJ_WIN32_WINCE */
	}
    #if defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8
	/* Avoid compile warning. */
	goto get_sdk_info;
    #endif
    }
#elif defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    {
	pj_symbianos_get_model_info(si_buffer, sizeof(si_buffer));
	ALLOC_CP_STR(si_buffer, machine);
	
	char *p = si_buffer + sizeof(si_buffer) - left;
	unsigned plen;
	plen = pj_symbianos_get_platform_info(p, left);
	if (plen) {
	    /* Output format will be "Series60vX.X" */
	    si.os_name = pj_str("S60");
	    si.os_ver  = parse_version(p+9);
	} else {
	    si.os_name = pj_str("Unknown");
	}
	
	/* Avoid compile warning on Symbian. */
	goto get_sdk_info;
    }
#endif

    /*
     * SDK info.
     */
get_sdk_info:

#if defined(__GLIBC__)
    si.sdk_ver = (__GLIBC__ << 24) |
		 (__GLIBC_MINOR__ << 16);
    si.sdk_name = pj_str("glibc");
#elif defined(__GNU_LIBRARY__)
    si.sdk_ver = (__GNU_LIBRARY__ << 24) |
	         (__GNU_LIBRARY_MINOR__ << 16);
    si.sdk_name = pj_str("libc");
#elif defined(__UCLIBC__)
    si.sdk_ver = (__UCLIBC_MAJOR__ << 24) |
    	         (__UCLIBC_MINOR__ << 16);
    si.sdk_name = pj_str("uclibc");
#elif defined(_WIN32_WCE) && _WIN32_WCE
    /* Old window mobile declares _WIN32_WCE as decimal (e.g. 300, 420, etc.),
     * but then it was changed to use hex, e.g. 0x420, etc. See
     * http://social.msdn.microsoft.com/forums/en-US/vssmartdevicesnative/thread/8a97c59f-5a1c-4bc6-99e6-427f065ff439/
     */
    #if _WIN32_WCE <= 500
	si.sdk_ver = ( (_WIN32_WCE / 100) << 24) |
		     ( ((_WIN32_WCE % 100) / 10) << 16) |
		     ( (_WIN32_WCE % 10) << 8);
    #else
	si.sdk_ver = ( ((_WIN32_WCE & 0xFF00) >> 8) << 24) |
		     ( ((_WIN32_WCE & 0x00F0) >> 4) << 16) |
		     ( ((_WIN32_WCE & 0x000F) >> 0) << 8);
    #endif
    si.sdk_name = pj_str("cesdk");
#elif defined(_MSC_VER)
    /* No SDK info is easily obtainable for Visual C, so lets just use
     * _MSC_VER. The _MSC_VER macro reports the major and minor versions
     * of the compiler. For example, 1310 for Microsoft Visual C++ .NET 2003.
     * 1310 represents version 13 and a 1.0 point release.
     * The Visual C++ 2005 compiler version is 1400.
     */
    si.sdk_ver = ((_MSC_VER / 100) << 24) |
    	         (((_MSC_VER % 100) / 10) << 16) |
    	         ((_MSC_VER % 10) << 8);
    si.sdk_name = pj_str("msvc");
#elif defined(PJ_SYMBIAN) && PJ_SYMBIAN != 0
    pj_symbianos_get_sdk_info(&si.sdk_name, &si.sdk_ver);
#endif

    /*
     * Build the info string.
     */
    {
	char tmp[PJ_SYS_INFO_BUFFER_SIZE];
	char os_ver[20], sdk_ver[20];
	int cnt;

	cnt = pj_ansi_snprintf(tmp, sizeof(tmp),
			       "%s%s%s%s%s%s%s",
			       si.os_name.ptr,
			       ver_info(si.os_ver, os_ver),
			       (si.machine.slen ? "/" : ""),
			       si.machine.ptr,
			       (si.sdk_name.slen ? "/" : ""),
			       si.sdk_name.ptr,
			       ver_info(si.sdk_ver, sdk_ver));
	if (cnt > 0 && cnt < (int)sizeof(tmp)) {
	    ALLOC_CP_STR(tmp, info);
	}
    }

    si_initialized = PJ_TRUE;
    return &si;
}

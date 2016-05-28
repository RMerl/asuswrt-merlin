/* 
   Unix SMB/CIFS implementation.
   SMB parameters and setup, plus a whole lot more.
   
   Copyright (C) Andrew Tridgell              2001
   
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

#ifndef _NT_STATUS_H
#define _NT_STATUS_H

/* The Splint code analysis tool doesn't like immediate structures. */

#ifdef _SPLINT_                      /* http://www.splint.org */
#undef HAVE_IMMEDIATE_STRUCTURES
#endif

/* the following rather strange looking definitions of NTSTATUS and WERROR
   and there in order to catch common coding errors where different error types
   are mixed up. This is especially important as we slowly convert Samba
   from using BOOL for internal functions 
*/

#if defined(HAVE_IMMEDIATE_STRUCTURES)
typedef struct {uint32 v;} NTSTATUS;
#define NT_STATUS(x) ((NTSTATUS) { x })
#define NT_STATUS_V(x) ((x).v)
#else
typedef uint32 NTSTATUS;
#define NT_STATUS(x) (x)
#define NT_STATUS_V(x) (x)
#endif

#if defined(HAVE_IMMEDIATE_STRUCTURES)
typedef struct {uint32 w;} WERROR;
#define W_ERROR(x) ((WERROR) { x })
#define W_ERROR_V(x) ((x).w)
#else
typedef uint32 WERROR;
#define W_ERROR(x) (x)
#define W_ERROR_V(x) (x)
#endif

#define NT_STATUS_IS_OK(x) (NT_STATUS_V(x) == 0)
#define NT_STATUS_IS_ERR(x) ((NT_STATUS_V(x) & 0xc0000000) == 0xc0000000)
#define NT_STATUS_EQUAL(x,y) (NT_STATUS_V(x) == NT_STATUS_V(y))
#define W_ERROR_IS_OK(x) (W_ERROR_V(x) == 0)
#define W_ERROR_EQUAL(x,y) (W_ERROR_V(x) == W_ERROR_V(y))

#define NT_STATUS_HAVE_NO_MEMORY(x) do { \
        if (!(x)) {\
                return NT_STATUS_NO_MEMORY;\
        }\
} while (0)

#define NT_STATUS_NOT_OK_RETURN(x) do { \
	if (!NT_STATUS_IS_OK(x)) {\
		return x;\
	}\
} while (0)

/* The top byte in an NTSTATUS code is used as a type field.
 * Windows only uses value 0xC0 as an indicator for an NT error
 * and 0x00 for success.
 * So we can use the type field to store other types of error codes
 * inside the three lower bytes. 
 * NB: The system error codes (errno) are not integrated via a type of
 *     their own but are mapped to genuine NT error codes via 
 *     map_nt_error_from_unix() */

#define NT_STATUS_TYPE(status) ((NT_STATUS_V(status) & 0xFF000000) >> 24)

#define NT_STATUS_TYPE_DOS  0xF1
#define NT_STATUS_TYPE_LDAP 0xF2

/* this defines special NTSTATUS codes to represent DOS errors.  I
   have chosen this macro to produce status codes in the invalid
   NTSTATUS range */
#define NT_STATUS_DOS_MASK (NT_STATUS_TYPE_DOS << 24)
#define NT_STATUS_DOS(class, code) NT_STATUS(NT_STATUS_DOS_MASK | ((class)<<16) | code)
#define NT_STATUS_IS_DOS(status) ((NT_STATUS_V(status) & 0xFF000000) == NT_STATUS_DOS_MASK)
#define NT_STATUS_DOS_CLASS(status) ((NT_STATUS_V(status) >> 16) & 0xFF)
#define NT_STATUS_DOS_CODE(status) (NT_STATUS_V(status) & 0xFFFF)

/* define ldap error codes as NTSTATUS codes */
#define NT_STATUS_LDAP_MASK (NT_STATUS_TYPE_LDAP << 24)
#define NT_STATUS_LDAP(code) NT_STATUS(NT_STATUS_LDAP_MASK | code)
#define NT_STATUS_IS_LDAP(status) ((NT_STATUS_V(status) & 0xFF000000) == NT_STATUS_LDAP_MASK)
#define NT_STATUS_LDAP_CODE(status) (NT_STATUS_V(status) & ~0xFF000000)

#endif

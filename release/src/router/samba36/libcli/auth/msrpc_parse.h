/*
   Unix SMB/CIFS implementation.
   simple kerberos5/SPNEGO routines
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002
   Copyright (C) Andrew Bartlett 2002-2003

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBCLI_AUTH_MSRPC_PARSE_H__
#define _LIBCLI_AUTH_MSRPC_PARSE_H__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

/* this file contains prototypes for functions that are private 
 * to this subsystem or library. These functions should not be 
 * used outside this particular subsystem! */


/* The following definitions come from /home/jeremy/src/samba/git/master/source3/../source4/../libcli/auth/msrpc_parse.c  */

NTSTATUS msrpc_gen(TALLOC_CTX *mem_ctx, 
	       DATA_BLOB *blob,
	       const char *format, ...);

/**
  this is a tiny msrpc packet parser. This the the partner of msrpc_gen

  format specifiers are:

  U = unicode string (output is unix string)
  A = ascii string
  B = data blob
  b = data blob in header
  d = word (4 bytes)
  C = constant ascii string
 */
bool msrpc_parse(TALLOC_CTX *mem_ctx, 
		 const DATA_BLOB *blob,
		 const char *format, ...);
#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2)

#endif


#ifndef _LIBCLI_AUTH_MSRPC_PARSE_H__
#define _LIBCLI_AUTH_MSRPC_PARSE_H__

#undef _PRINTF_ATTRIBUTE
#define _PRINTF_ATTRIBUTE(a1, a2) PRINTF_ATTRIBUTE(a1, a2)

/* this file contains prototypes for functions that are private 
 * to this subsystem or library. These functions should not be 
 * used outside this particular subsystem! */


/* The following definitions come from /home/jeremy/src/samba/git/master/source3/../source4/../libcli/auth/msrpc_parse.c  */

bool msrpc_gen(TALLOC_CTX *mem_ctx, 
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


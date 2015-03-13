#ifndef __HTTP_NTLM_H
#define __HTTP_NTLM_H
/* Declarations for http_ntlm.c
   Copyright (C) 1995, 1996, 1997, 2000, 2007, 2008, 2009, 2010, 2011
   Free Software Foundation, Inc.
   Contributed by Daniel Stenberg.

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

typedef enum {
  NTLMSTATE_NONE,
  NTLMSTATE_TYPE1,
  NTLMSTATE_TYPE2,
  NTLMSTATE_TYPE3,
  NTLMSTATE_LAST
} wgetntlm;

/* Struct used for NTLM challenge-response authentication */
struct ntlmdata {
  wgetntlm state;
  unsigned char nonce[8];
};

/* this is for ntlm header input */
bool ntlm_input (struct ntlmdata *, const char *);

/* this is for creating ntlm header output */
char *ntlm_output (struct ntlmdata *, const char *, const char *, bool *);
#endif

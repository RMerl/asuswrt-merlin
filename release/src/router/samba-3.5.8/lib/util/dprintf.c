/* 
   Unix SMB/CIFS implementation.
   display print functions
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Jelmer Vernooij 2007
   
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


/*
  this module provides functions for printing internal strings in the 
  "display charset".
  
  This charset may be quite different from the chosen unix charset.

  Eventually these functions will need to take care of column count constraints

  The d_ prefix on print functions in Samba refers to the display character set
  conversion
*/

#include "includes.h"
#include "system/locale.h"
#include "param/param.h"

static smb_iconv_t display_cd = (smb_iconv_t)-1;

void d_set_iconv(smb_iconv_t cd)
{
	if (display_cd != (smb_iconv_t)-1)
		talloc_free(display_cd);

	display_cd = cd;
}

_PUBLIC_ int d_vfprintf(FILE *f, const char *format, va_list ap) 
{
	char *p, *p2;
	int ret, clen;
	va_list ap2;

	/* If there's nothing to convert, take a shortcut */
	if (display_cd == (smb_iconv_t)-1) {
		return vfprintf(f, format, ap);
	}

	/* do any message translations */
	va_copy(ap2, ap);
	ret = vasprintf(&p, format, ap2);
	va_end(ap2);

	if (ret <= 0) return ret;

	clen = iconv_talloc(NULL, display_cd, p, ret, (void **)&p2);
        if (clen == -1) {
		/* the string can't be converted - do the best we can,
		   filling in non-printing chars with '?' */
		int i;
		for (i=0;i<ret;i++) {
			if (isprint(p[i]) || isspace(p[i])) {
				fwrite(p+i, 1, 1, f);
			} else {
				fwrite("?", 1, 1, f);
			}
		}
		SAFE_FREE(p);
		return ret;
        }

	/* good, its converted OK */
	SAFE_FREE(p);
	ret = fwrite(p2, 1, clen, f);
	talloc_free(p2);

	return ret;
}


_PUBLIC_ int d_fprintf(FILE *f, const char *format, ...) 
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = d_vfprintf(f, format, ap);
	va_end(ap);

	return ret;
}

_PUBLIC_ int d_printf(const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = d_vfprintf(stdout, format, ap);
	va_end(ap);

	return ret;
}


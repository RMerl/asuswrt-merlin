/*
 * Samba Unix/Linux SMB client library
 *
 * Copyright (C) Gregor Beck 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   srprs.c
 * @author Gregor Beck <gb@sernet.de>
 * @date   Aug 2010
 * @brief  A simple recursive parser.
 */

#include "srprs.h"
#include "cbuf.h"
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

bool srprs_skipws(const char** ptr) {
	while (isspace(**ptr))
		++(*ptr);
	return true;
}

bool srprs_char(const char** ptr, char c) {
	if (**ptr == c) {
		++(*ptr);
		return true;
	}
	return false;
}

bool srprs_str(const char** ptr, const char* str, size_t len)
{
	if (len == -1)
		len = strlen(str);

	if (memcmp(*ptr, str, len) == 0) {
		*ptr += len;
		return true;
	}
	return false;
}

bool srprs_charset(const char** ptr, const char* set, cbuf* oss)
{
	const char* p = strchr(set, **ptr);
	if (p != NULL && *p != '\0') {
		cbuf_putc(oss, **ptr);
		++(*ptr);
		return true;
	}
	return false;
}

bool srprs_charsetinv(const char** ptr, const char* set, cbuf* oss)
{
	if ((**ptr != '\0') && (strchr(set, **ptr) == NULL)) {
		cbuf_putc(oss, **ptr);
		++(*ptr);
		return true;
	}
	return false;
}



bool srprs_quoted_string(const char** ptr, cbuf* str, bool* cont)
{
	const char* pos = *ptr;
	const size_t spos = cbuf_getpos(str);

	if (cont == NULL || *cont == false) {
		if (!srprs_char(&pos, '\"'))
			goto fail;
	}

	while (true) {
		while (srprs_charsetinv(&pos, "\\\"", str))
			;

		switch (*pos) {
		case '\0':
			if (cont == NULL) {
				goto fail;
			} else {
				*ptr = pos;
				*cont = true;
				return true;
			}
		case '\"':
			*ptr  = pos+1;
			if (cont != NULL) {
				*cont = false;
			}
			return true;

		case '\\':
			pos++;
			if (!srprs_charset(&pos, "\\\"", str))
				goto fail;
			break;

		default:
			assert(false);
		}
	}

fail:
	cbuf_setpos(str, spos);
	return false;
}

bool srprs_hex(const char** ptr, size_t len, unsigned* u)
{
	static const char* FMT[] = {
		"%1x","%2x","%3x","%4x","%5x","%6x","%7x","%8x",
		"%9x","%10x","%11x","%12x","%13x","%14x","%15x","%16x"
	};

	const char* pos = *ptr;
	int ret;
	int i;

	assert((len > 0)
	       && (len <= 2*sizeof(unsigned))
	       && (len <= sizeof(FMT)/sizeof(const char*)));

	for (i=0; i<len; i++) {
		if (!srprs_charset(&pos, "0123456789abcdefABCDEF", NULL)) {
			break;
		}
	}

	ret = sscanf(*ptr, FMT[len-1], u);

	if ( ret != 1 ) {
		return false;
	}

	*ptr = pos;
	return true;
}

bool srprs_nl(const char** ptr, cbuf* nl)
{
	static const char CRLF[] = "\r\n";
	if (srprs_str(ptr, CRLF, sizeof(CRLF) - 1)) {
		cbuf_puts(nl, CRLF, sizeof(CRLF) - 1);
		return true;
	}
	return srprs_charset(ptr, "\n\r", nl);
}

bool srprs_eos(const char** ptr)
{
	return (**ptr == '\0');
}

bool srprs_eol(const char** ptr, cbuf* nl)
{
	return  srprs_eos(ptr) || srprs_nl(ptr, nl);
}

bool srprs_line(const char** ptr, cbuf* str)
{
	while (srprs_charsetinv(ptr, "\n\r", str))
		;
	return true;
}

bool srprs_quoted(const char** ptr, cbuf* str)
{
	const char* pos = *ptr;
	const size_t spos = cbuf_getpos(str);

	if (!srprs_char(&pos, '"')) {
		goto fail;
	}

	while (true) {
		while (srprs_charsetinv(&pos, "\\\"", str))
			;

		switch (*pos) {
		case '\0':
			goto fail;
		case '"':
			*ptr  = pos+1;
			return true;

		case '\\':
			pos++;
			if (!srprs_charset(&pos, "\\\"", str)) {
				unsigned u;
				if (!srprs_hex(&pos, 2, &u)) {
					goto fail;
				}
				cbuf_putc(str, u);
			}
			break;
		default:
			assert(false);
		}
	}

fail:
	cbuf_setpos(str, spos);
	return false;
}

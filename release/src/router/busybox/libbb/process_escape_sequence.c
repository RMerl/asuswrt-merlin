/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) Manuel Novoa III <mjn3@codepoet.org>
 * and Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define WANT_HEX_ESCAPES 1

/* Usual "this only works for ascii compatible encodings" disclaimer. */
#undef _tolower
#define _tolower(X) ((X)|((char) 0x20))

char FAST_FUNC bb_process_escape_sequence(const char **ptr)
{
	/* bash builtin "echo -e '\ec'" interprets \e as ESC,
	 * but coreutils "/bin/echo -e '\ec'" does not.
	 * manpages tend to support coreutils way.
	 * Update: coreutils added support for \e on 28 Oct 2009. */
	static const char charmap[] ALIGN1 = {
		'a',  'b', 'e', 'f',  'n',  'r',  't',  'v',  '\\', 0,
		'\a', '\b', 27, '\f', '\n', '\r', '\t', '\v', '\\', '\\' };

	const char *p;
	const char *q;
	unsigned num_digits;
	unsigned r;
	unsigned n;
	unsigned d;
	unsigned base;

	num_digits = n = 0;
	base = 8;
	q = *ptr;

#ifdef WANT_HEX_ESCAPES
	if (*q == 'x') {
		++q;
		base = 16;
		++num_digits;
	}
#endif

	/* bash requires leading 0 in octal escapes:
	 * \02 works, \2 does not (prints \ and 2).
	 * We treat \2 as a valid octal escape sequence. */
	do {
		d = (unsigned char)(*q) - '0';
#ifdef WANT_HEX_ESCAPES
		if (d >= 10) {
			d = (unsigned char)(_tolower(*q)) - 'a' + 10;
		}
#endif

		if (d >= base) {
#ifdef WANT_HEX_ESCAPES
			if ((base == 16) && (!--num_digits)) {
/*				return '\\'; */
				--q;
			}
#endif
			break;
		}

		r = n * base + d;
		if (r > UCHAR_MAX) {
			break;
		}

		n = r;
		++q;
	} while (++num_digits < 3);

	if (num_digits == 0) {	/* mnemonic escape sequence? */
		p = charmap;
		do {
			if (*p == *q) {
				q++;
				break;
			}
		} while (*++p);
		/* p points to found escape char or NUL,
		 * advance it and find what it translates to */
		p += sizeof(charmap) / 2;
		n = *p;
	}

	*ptr = q;

	return (char) n;
}

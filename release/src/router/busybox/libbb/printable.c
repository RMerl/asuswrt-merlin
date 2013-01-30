/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

void FAST_FUNC fputc_printable(int ch, FILE *file)
{
	if ((ch & (0x80 + PRINTABLE_META)) == (0x80 + PRINTABLE_META)) {
		fputs("M-", file);
		ch &= 0x7f;
	}
	ch = (unsigned char) ch;
	if (ch == 0x9b) {
		/* VT100's CSI, aka Meta-ESC, is not printable on vt-100 */
		ch = '{';
		goto print_caret;
	}
	if (ch < ' ') {
		ch += '@';
		goto print_caret;
	}
	if (ch == 0x7f) {
		ch = '?';
 print_caret:
		fputc('^', file);
	}
	fputc(ch, file);
}

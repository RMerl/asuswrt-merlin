/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"

#if defined(__GLIBC__) && __GLIBC__ < 2
int FAST_FUNC vdprintf(int d, const char *format, va_list ap)
{
	char buf[8 * 1024];
	int len;

	len = vsnprintf(buf, sizeof(buf), format, ap);
	return write(d, buf, len);
}
#endif

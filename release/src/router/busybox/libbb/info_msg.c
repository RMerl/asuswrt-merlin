/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#include "libbb.h"
#if ENABLE_FEATURE_SYSLOG
# include <syslog.h>
#endif

void FAST_FUNC bb_info_msg(const char *s, ...)
{
#ifdef THIS_ONE_DOESNT_DO_SINGLE_WRITE
	va_list p;
	/* va_copy is used because it is not portable
	 * to use va_list p twice */
	va_list p2;

	va_start(p, s);
	va_copy(p2, p);
	if (logmode & LOGMODE_STDIO) {
		vprintf(s, p);
		fputs(msg_eol, stdout);
	}
# if ENABLE_FEATURE_SYSLOG
	if (logmode & LOGMODE_SYSLOG)
		vsyslog(LOG_INFO, s, p2);
# endif
	va_end(p2);
	va_end(p);
#else
	int used;
	char *msg;
	va_list p;

	if (logmode == 0)
		return;

	va_start(p, s);
	used = vasprintf(&msg, s, p);
	va_end(p);
	if (used < 0)
		return;

# if ENABLE_FEATURE_SYSLOG
	if (logmode & LOGMODE_SYSLOG)
		syslog(LOG_INFO, "%s", msg);
# endif
	if (logmode & LOGMODE_STDIO) {
		fflush_all();
		/* used = strlen(msg); - must be true already */
		msg[used++] = '\n';
		full_write(STDOUT_FILENO, msg, used);
	}

	free(msg);
#endif
}

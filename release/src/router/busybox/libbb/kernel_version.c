/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
/* After libbb.h, since it needs sys/types.h on some systems */
#include <sys/utsname.h>  /* for uname(2) */


/* Returns current kernel version encoded as major*65536 + minor*256 + patch,
 * so, for example,  to check if the kernel is greater than 2.2.11:
 *
 *     if (get_linux_version_code() > KERNEL_VERSION(2,2,11)) { <stuff> }
 */
int FAST_FUNC get_linux_version_code(void)
{
	struct utsname name;
	char *s;
	int i, r;

	if (uname(&name) == -1) {
		bb_perror_msg("can't get system information");
		return 0;
	}

	s = name.release;
	r = 0;
	for (i = 0; i < 3; i++) {
		r = r * 256 + atoi(strtok(s, "."));
		s = NULL;
	}
	return r;
}

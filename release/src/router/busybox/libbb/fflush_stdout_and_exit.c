/* vi: set sw=4 ts=4: */
/*
 * fflush_stdout_and_exit implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* Attempt to fflush(stdout), and exit with an error code if stdout is
 * in an error state.
 */

#include "libbb.h"

void FAST_FUNC fflush_stdout_and_exit(int retval)
{
	if (fflush(stdout))
		bb_perror_msg_and_die(bb_msg_standard_output);

	if (ENABLE_FEATURE_PREFER_APPLETS && die_sleep < 0) {
		/* We are in NOFORK applet. Do not exit() directly,
		 * but use xfunc_die() */
		xfunc_error_retval = retval;
		xfunc_die();
	}

	exit(retval);
}

/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2009 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

char* FAST_FUNC single_argv(char **argv)
{
	if (!argv[1] || argv[2])
		bb_show_usage();
	return argv[1];
}

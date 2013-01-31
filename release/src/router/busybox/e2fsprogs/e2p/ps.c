/* vi: set sw=4 ts=4: */
/*
 * ps.c			- Print filesystem state
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/12/22	- Creation
 */

#include <stdio.h>

#include "e2p.h"

void print_fs_state(FILE *f, unsigned short state)
{
	fprintf(f, (state & EXT2_VALID_FS ? " clean" : " not clean"));
	if (state & EXT2_ERROR_FS)
		fprintf(f, " with errors");
}

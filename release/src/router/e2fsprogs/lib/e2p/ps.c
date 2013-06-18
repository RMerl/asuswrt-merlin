/*
 * ps.c			- Print filesystem state
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

/*
 * History:
 * 93/12/22	- Creation
 */

#include "config.h"
#include <stdio.h>

#include "e2p.h"

void print_fs_state (FILE * f, unsigned short state)
{
	if (state & EXT2_VALID_FS)
		fprintf (f, " clean");
	else
		fprintf (f, " not clean");
	if (state & EXT2_ERROR_FS)
		fprintf (f, " with errors");
}

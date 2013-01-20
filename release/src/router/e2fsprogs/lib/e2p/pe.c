/*
 * pe.c			- Print a second extended filesystem errors behavior
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

/*
 * History:
 * 94/01/09	- Creation
 */

#include <stdio.h>

#include "e2p.h"

void print_fs_errors (FILE * f, unsigned short errors)
{
	switch (errors)
	{
		case EXT2_ERRORS_CONTINUE:
			fprintf (f, "Continue");
			break;
		case EXT2_ERRORS_RO:
			fprintf (f, "Remount read-only");
			break;
		case EXT2_ERRORS_PANIC:
			fprintf (f, "Panic");
			break;
		default:
			fprintf (f, "Unknown (continue)");
	}
}

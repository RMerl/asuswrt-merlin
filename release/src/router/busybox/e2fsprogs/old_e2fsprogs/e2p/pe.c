/* vi: set sw=4 ts=4: */
/*
 * pe.c			- Print a second extended filesystem errors behavior
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 94/01/09	- Creation
 */

#include <stdio.h>

#include "e2p.h"

void print_fs_errors(FILE *f, unsigned short errors)
{
	char *disp = NULL;
	switch (errors) {
	case EXT2_ERRORS_CONTINUE: disp = "Continue"; break;
	case EXT2_ERRORS_RO:       disp = "Remount read-only"; break;
	case EXT2_ERRORS_PANIC:    disp = "Panic"; break;
	default:                   disp = "Unknown (continue)";
	}
	fprintf(f, disp);
}

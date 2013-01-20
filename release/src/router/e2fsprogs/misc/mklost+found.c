/*
 * mklost+found.c	- Creates a directory lost+found on a mounted second
 *			  extended file system
 *
 * Copyright (C) 1992, 1993  Remy Card <card@masi.ibp.fr>
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */

/*
 * History:
 * 93/04/22	- Creation
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "ext2fs/ext2_fs.h"
#include "../version.h"
#include "nls-enable.h"

#define LPF "lost+found"

int main (int argc, char ** argv)
{
	char name [EXT2_NAME_LEN];
	char path [sizeof (LPF) + 1 + 256];
	struct stat st;
	int i, j;
	int d;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	fprintf (stderr, "mklost+found %s (%s)\n", E2FSPROGS_VERSION,
		 E2FSPROGS_DATE);
	if (argc != 1) {
		(void)argv; /* avoid unused argument warning */
		fprintf (stderr, _("Usage: mklost+found\n"));
		exit(1);
	}
	if (mkdir (LPF, 0700) == -1) {
		perror ("mkdir");
		exit(1);
	}

	i = 0;
	memset (name, 'x', 246);
	do {
		sprintf (name + 246, "%08d", i);
		strcpy (path, LPF);
		strcat (path, "/");
		strcat (path, name);
		if ((d = creat (path, 0644)) == -1) {
			perror ("creat");
			exit (1);
		}
		i++;
		close (d);
		if (stat (LPF, &st) == -1) {
			perror ("stat");
			exit (1);
		}
	} while (st.st_size <= (EXT2_NDIR_BLOCKS - 1) * st.st_blksize);
	for (j = 0; j < i; j++) {
		sprintf (name + 246, "%08d", j);
		strcpy (path, LPF);
		strcat (path, "/");
		strcat (path, name);
		if (unlink (path) == -1) {
			perror ("unlink");
			exit (1);
		}
	}
	exit (0);
}

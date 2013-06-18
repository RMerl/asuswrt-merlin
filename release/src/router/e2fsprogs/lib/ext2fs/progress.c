/*
 * progress.c - Numeric progress meter
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
 * 	2003, 2004, 2005 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include "ext2fs.h"
#include "ext2fsP.h"

#include <time.h>

static char spaces[80], backspaces[80];
static time_t last_update;

static int int_log10(unsigned int arg)
{
	int	l;

	for (l=0; arg ; l++)
		arg = arg / 10;
	return l;
}

void ext2fs_numeric_progress_init(ext2_filsys fs,
				  struct ext2fs_numeric_progress_struct * progress,
				  const char *label, __u64 max)
{
	/*
	 * The PRINT_PROGRESS flag turns on or off ALL
	 * progress-related messages, whereas the SKIP_PROGRESS
	 * environment variable prints the start and end messages but
	 * not the numeric countdown in the middle.
	 */
	if (!(fs->flags & EXT2_FLAG_PRINT_PROGRESS))
		return;

	memset(spaces, ' ', sizeof(spaces)-1);
	spaces[sizeof(spaces)-1] = 0;
	memset(backspaces, '\b', sizeof(backspaces)-1);
	backspaces[sizeof(backspaces)-1] = 0;

	memset(progress, 0, sizeof(*progress));
	if (getenv("E2FSPROGS_SKIP_PROGRESS"))
		progress->skip_progress++;


	/*
	 * Figure out how many digits we need
	 */
	progress->max = max;
	progress->log_max = int_log10(max);

	if (label) {
		fputs(label, stdout);
		fflush(stdout);
	}
	last_update = 0;
}

void ext2fs_numeric_progress_update(ext2_filsys fs,
				    struct ext2fs_numeric_progress_struct * progress,
				    __u64 val)
{
	time_t now;

	if (!(fs->flags & EXT2_FLAG_PRINT_PROGRESS))
		return;
	if (progress->skip_progress)
		return;
	now = time(0);
	if (now == last_update)
		return;
	last_update = now;

	printf("%*llu/%*llu", progress->log_max, val,
	       progress->log_max, progress->max);
	fprintf(stdout, "%.*s", (2*progress->log_max)+1, backspaces);
}

void ext2fs_numeric_progress_close(ext2_filsys fs,
				   struct ext2fs_numeric_progress_struct * progress,
				   const char *message)
{
	if (!(fs->flags & EXT2_FLAG_PRINT_PROGRESS))
		return;
	fprintf(stdout, "%.*s", (2*progress->log_max)+1, spaces);
	fprintf(stdout, "%.*s", (2*progress->log_max)+1, backspaces);
	if (message)
		fputs(message, stdout);
}

/*
 * sim_progress.c --- simple progress meter
 *
 * Copyright (C) 1997, 1998 by Theodore Ts'o and
 * 	PowerQuest, Inc.
 *
 * Copyright (C) 1999, 2000 by Theosore Ts'o
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "resize2fs.h"

struct ext2_sim_progress {
	FILE	*f;
	char	*label;
	int	labelwidth;
	int	barwidth;
	__u32	maxdone;
	__u32	current;
	int	shown;
	int	flags;
};

static errcode_t ext2fs_progress_display(ext2_sim_progmeter prog)
{
	int	i, width;

	fputs(prog->label, prog->f);
	width = prog->labelwidth - strlen(prog->label);
	while (width-- > 0)
		putc(' ', prog->f);
	if (prog->labelwidth + prog->barwidth > 80) {
		fputs("\n", prog->f);
		for (width = prog->labelwidth; width > 0; width--)
			putc(' ', prog->f);
	}
	for (i=0; i < prog->barwidth; i++)
		putc('-', prog->f);
	for (i=0; i < prog->barwidth; i++)
		putc('\b', prog->f);
	fflush(prog->f);
	return 0;
}


void ext2fs_progress_update(ext2_sim_progmeter prog, __u32 current)
{
	int		old_level, level, num, i;

	level = prog->barwidth * current / prog->maxdone;
	old_level = prog->barwidth * prog->current / prog->maxdone;
	prog->current = current;

	num = level - old_level;
	if (num == 0)
		return;

	if (num > 0) {
		for (i=0; i < num; i++)
			putc('X', prog->f);
	} else {
		num = -num;
		for (i=0; i < num; i++)
			putc('\b', prog->f);
		for (i=0; i < num; i++)
			putc('-', prog->f);
		for (i=0; i < num; i++)
			putc('\b', prog->f);
	}
	fflush(prog->f);
}

errcode_t ext2fs_progress_init(ext2_sim_progmeter *ret_prog,
			       const char *label,
			       int labelwidth, int barwidth,
			       __u32 maxdone, int flags)
{
	ext2_sim_progmeter	prog;
	errcode_t		retval;

	retval = ext2fs_get_mem(sizeof(struct ext2_sim_progress), &prog);
	if (retval)
		return retval;
	memset(prog, 0, sizeof(struct ext2_sim_progress));

	retval = ext2fs_get_mem(strlen(label)+1, &prog->label);
	if (retval) {
		free(prog);
		return retval;
	}
	strcpy(prog->label, label);
	prog->labelwidth = labelwidth;
	prog->barwidth = barwidth;
	prog->flags = flags;
	prog->maxdone = maxdone;
	prog->current = 0;
	prog->shown = 0;
	prog->f = stdout;

	*ret_prog = prog;

	return ext2fs_progress_display(prog);
}

void ext2fs_progress_close(ext2_sim_progmeter prog)
{

	if (prog->label)
		ext2fs_free_mem(&prog->label);
	ext2fs_free_mem(&prog);
	printf("\n");
	return;
}

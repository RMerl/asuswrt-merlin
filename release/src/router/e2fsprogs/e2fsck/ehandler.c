/*
 * ehandler.c --- handle bad block errors which come up during the
 * 	course of an e2fsck session.
 *
 * Copyright (C) 1994 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>

#include "e2fsck.h"

#include <sys/time.h>
#include <sys/resource.h>

static const char *operation;

static errcode_t e2fsck_handle_read_error(io_channel channel,
					  unsigned long block,
					  int count,
					  void *data,
					  size_t size EXT2FS_ATTR((unused)),
					  int actual EXT2FS_ATTR((unused)),
					  errcode_t error)
{
	int	i;
	char	*p;
	ext2_filsys fs = (ext2_filsys) channel->app_data;
	e2fsck_t ctx;

	ctx = (e2fsck_t) fs->priv_data;
	if (ctx->flags & E2F_FLAG_EXITING)
		return 0;
	/*
	 * If more than one block was read, try reading each block
	 * separately.  We could use the actual bytes read to figure
	 * out where to start, but we don't bother.
	 */
	if (count > 1) {
		p = (char *) data;
		for (i=0; i < count; i++, p += channel->block_size, block++) {
			error = io_channel_read_blk64(channel, block,
						    1, p);
			if (error)
				return error;
		}
		return 0;
	}
	if (operation)
		printf(_("Error reading block %lu (%s) while %s.  "), block,
		       error_message(error), operation);
	else
		printf(_("Error reading block %lu (%s).  "), block,
		       error_message(error));
	preenhalt(ctx);
	if (ask(ctx, _("Ignore error"), 1)) {
		if (ask(ctx, _("Force rewrite"), 1))
			io_channel_write_blk64(channel, block, count, data);
		return 0;
	}

	return error;
}

static errcode_t e2fsck_handle_write_error(io_channel channel,
					    unsigned long block,
					    int count,
					    const void *data,
					    size_t size EXT2FS_ATTR((unused)),
					    int actual EXT2FS_ATTR((unused)),
					    errcode_t error)
{
	int		i;
	const char	*p;
	ext2_filsys fs = (ext2_filsys) channel->app_data;
	e2fsck_t ctx;

	ctx = (e2fsck_t) fs->priv_data;
	if (ctx->flags & E2F_FLAG_EXITING)
		return 0;

	/*
	 * If more than one block was written, try writing each block
	 * separately.  We could use the actual bytes read to figure
	 * out where to start, but we don't bother.
	 */
	if (count > 1) {
		p = (const char *) data;
		for (i=0; i < count; i++, p += channel->block_size, block++) {
			error = io_channel_write_blk64(channel, block,
						     1, p);
			if (error)
				return error;
		}
		return 0;
	}

	if (operation)
		printf(_("Error writing block %lu (%s) while %s.  "), block,
		       error_message(error), operation);
	else
		printf(_("Error writing block %lu (%s).  "), block,
		       error_message(error));
	preenhalt(ctx);
	if (ask(ctx, _("Ignore error"), 1))
		return 0;

	return error;
}

const char *ehandler_operation(const char *op)
{
	const char *ret = operation;

	operation = op;
	return ret;
}

void ehandler_init(io_channel channel)
{
	channel->read_error = e2fsck_handle_read_error;
	channel->write_error = e2fsck_handle_write_error;
}

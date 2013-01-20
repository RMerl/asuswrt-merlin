/*
 * e2undo.c - Replay an undo log onto an ext2/3/4 filesystem
 *
 * Copyright IBM Corporation, 2007
 * Author Aneesh Kumar K.V <aneesh.kumar@linux.vnet.ibm.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fcntl.h>
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include "ext2fs/tdb.h"
#include "ext2fs/ext2fs.h"
#include "nls-enable.h"

unsigned char mtime_key[] = "filesystem MTIME";
unsigned char uuid_key[] = "filesystem UUID";
unsigned char blksize_key[] = "filesystem BLKSIZE";

char *prg_name;

static void usage(char *prg_name)
{
	fprintf(stderr,
		_("Usage: %s <transaction file> <filesystem>\n"), prg_name);
	exit(1);

}

static int check_filesystem(TDB_CONTEXT *tdb, io_channel channel)
{
	__u32   s_mtime;
	__u8    s_uuid[16];
	errcode_t retval;
	TDB_DATA tdb_key, tdb_data;
	struct ext2_super_block super;

	io_channel_set_blksize(channel, SUPERBLOCK_OFFSET);
	retval = io_channel_read_blk(channel, 1, -SUPERBLOCK_SIZE, &super);
	if (retval) {
		com_err(prg_name,
			retval, _("Failed to read the file system data \n"));
		return retval;
	}

	tdb_key.dptr = mtime_key;
	tdb_key.dsize = sizeof(mtime_key);
	tdb_data = tdb_fetch(tdb, tdb_key);
	if (!tdb_data.dptr) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
		com_err(prg_name, retval,
			_("Failed tdb_fetch %s\n"), tdb_errorstr(tdb));
		return retval;
	}

	s_mtime = *(__u32 *)tdb_data.dptr;
	if (super.s_mtime != s_mtime) {

		com_err(prg_name, 0,
			_("The file system Mount time didn't match %u\n"),
			s_mtime);

		return  -1;
	}


	tdb_key.dptr = uuid_key;
	tdb_key.dsize = sizeof(uuid_key);
	tdb_data = tdb_fetch(tdb, tdb_key);
	if (!tdb_data.dptr) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
		com_err(prg_name, retval,
			_("Failed tdb_fetch %s\n"), tdb_errorstr(tdb));
		return retval;
	}
	memcpy(s_uuid, tdb_data.dptr, sizeof(s_uuid));
	if (memcmp(s_uuid, super.s_uuid, sizeof(s_uuid))) {
		com_err(prg_name, 0,
			_("The file system UUID didn't match \n"));
		return -1;
	}

	return 0;
}

static int set_blk_size(TDB_CONTEXT *tdb, io_channel channel)
{
	int block_size;
	errcode_t retval;
	TDB_DATA tdb_key, tdb_data;

	tdb_key.dptr = blksize_key;
	tdb_key.dsize = sizeof(blksize_key);
	tdb_data = tdb_fetch(tdb, tdb_key);
	if (!tdb_data.dptr) {
		retval = EXT2_ET_TDB_SUCCESS + tdb_error(tdb);
		com_err(prg_name, retval,
			_("Failed tdb_fetch %s\n"), tdb_errorstr(tdb));
		return retval;
	}

	block_size = *(int *)tdb_data.dptr;
#ifdef DEBUG
	printf("Block size %d\n", block_size);
#endif
	io_channel_set_blksize(channel, block_size);

	return 0;
}

int main(int argc, char *argv[])
{
	int c,force = 0;
	TDB_CONTEXT *tdb;
	TDB_DATA key, data;
	io_channel channel;
	errcode_t retval;
	int  mount_flags;
	unsigned long  blk_num;
	char *device_name, *tdb_file;
	io_manager manager = unix_io_manager;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
#endif
	add_error_table(&et_ext2_error_table);

	prg_name = argv[0];
	while((c = getopt(argc, argv, "f")) != EOF) {
		switch (c) {
			case 'f':
				force = 1;
				break;
			default:
				usage(prg_name);
		}
	}

	if (argc != optind+2)
		usage(prg_name);

	tdb_file = argv[optind];
	device_name = argv[optind+1];

	tdb = tdb_open(tdb_file, 0, 0, O_RDONLY, 0600);

	if (!tdb) {
		com_err(prg_name, errno,
				_("Failed tdb_open %s\n"), tdb_file);
		exit(1);
	}

	retval = ext2fs_check_if_mounted(device_name, &mount_flags);
	if (retval) {
		com_err(prg_name, retval, _("Error while determining whether "
				"%s is mounted.\n"), device_name);
		exit(1);
	}

	if (mount_flags & EXT2_MF_MOUNTED) {
		com_err(prg_name, retval, _("e2undo should only be run on "
				"unmounted file system\n"));
		exit(1);
	}

	retval = manager->open(device_name,
				IO_FLAG_EXCLUSIVE | IO_FLAG_RW,  &channel);
	if (retval) {
		com_err(prg_name, retval,
				_("Failed to open %s\n"), device_name);
		exit(1);
	}

	if (!force && check_filesystem(tdb, channel)) {
		exit(1);
	}

	if (set_blk_size(tdb, channel)) {
		exit(1);
	}

	for (key = tdb_firstkey(tdb); key.dptr; key = tdb_nextkey(tdb, key)) {
		if (!strcmp((char *) key.dptr, (char *) mtime_key) ||
		    !strcmp((char *) key.dptr, (char *) uuid_key) ||
		    !strcmp((char *) key.dptr, (char *) blksize_key)) {
			continue;
		}

		data = tdb_fetch(tdb, key);
		if (!data.dptr) {
			com_err(prg_name, 0,
				_("Failed tdb_fetch %s\n"), tdb_errorstr(tdb));
			exit(1);
		}
		blk_num = *(unsigned long *)key.dptr;
		printf(_("Replayed transaction of size %zd at location %ld\n"),
							data.dsize, blk_num);
		retval = io_channel_write_blk(channel, blk_num,
						-data.dsize, data.dptr);
		if (retval == -1) {
			com_err(prg_name, retval,
					_("Failed write %s\n"),
					strerror(errno));
			exit(1);
		}
	}
	io_channel_close(channel);
	tdb_close(tdb);

	return 0;
}

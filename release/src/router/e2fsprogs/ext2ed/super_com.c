/*

/usr/src/ext2ed/super_com.c

A part of the extended file system 2 disk editor.

----------------------
Handles the superblock
----------------------

First written on: April 9 1995

Copyright (C) 1995 Gadi Oxman

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ext2ed.h"

void type_ext2_super_block___show (char *command_line)

{
	struct ext2_super_block *super;
	super=&type_data.u.t_ext2_super_block;

	show (command_line);

	if (ext2fs_blocks_count(super) != 0) {
		wmove (show_pad,2,40);wprintw (show_pad,"%2.2f%%",100*(float) ext2fs_r_blocks_count(super)/ (float) ext2fs_blocks_count(super));
		wmove (show_pad,3,40);wprintw (show_pad,"%2.2f%%",100*(float) ext2fs_free_blocks_count(super)/ (float) ext2fs_blocks_count(super));
	}

	if (super->s_inodes_count != 0) {
		wmove (show_pad,4,40);wprintw (show_pad,"%2.2f%%",100*(float) super->s_free_inodes_count/ (float) super->s_inodes_count);
	}

	wmove (show_pad,6,40);
	switch (super->s_log_block_size) {
		case 0:	wprintw (show_pad,"1024 bytes");break;
		case 1: wprintw (show_pad,"2048 bytes");break;
		case 2: wprintw (show_pad,"4096 bytes");break;
	}
	wmove (show_pad,11,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_super_block.s_mtime));
	wmove (show_pad,12,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_super_block.s_wtime));
	wmove (show_pad,19,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_super_block.s_lastcheck));
	wmove (show_pad,15,40);

	switch (type_data.u.t_ext2_super_block.s_magic) {
		case EXT2_SUPER_MAGIC:
			wprintw (show_pad,"ext2 >= 0.2B");
			break;
		case EXT2_PRE_02B_MAGIC:
			wprintw (show_pad,"ext2 < 0.2B (not supported)");
			break;
		default:
			wprintw (show_pad,"Unknown");
			break;
	}

	wmove (show_pad,16,40);
	if (type_data.u.t_ext2_super_block.s_state & 0x1)
		wprintw (show_pad,"clean ");
	else
		wprintw (show_pad,"not clean ");

	if (type_data.u.t_ext2_super_block.s_state & 0x2)
		wprintw (show_pad,"with errors ");
	else
		wprintw (show_pad,"with no errors");

	wmove (show_pad,17,40);

	switch (type_data.u.t_ext2_super_block.s_errors) {
		case EXT2_ERRORS_CONTINUE:
			wprintw (show_pad,"Continue");
			break;
		case EXT2_ERRORS_RO:
			wprintw (show_pad,"Remount read only");
			break;
		case EXT2_ERRORS_PANIC:
			wprintw (show_pad,"Issue kernel panic");
			break;
		default:
			wprintw (show_pad,"Unknown");
			break;
	}

	wmove (show_pad,21,40);

	switch (type_data.u.t_ext2_super_block.s_creator_os) {

		case EXT2_OS_LINUX:
			wprintw (show_pad,"Linux :-)");
			break;

		case EXT2_OS_HURD:
			wprintw (show_pad,"Hurd");
			break;

		default:
			wprintw (show_pad,"Unknown");
			break;
	}

	refresh_show_pad ();

	wmove (show_win,1,0);wprintw (show_win,"\n");wmove (show_win,2,0);
	wprintw (show_win,"Superblock copy %ld ",super_info.copy_num);
	if (super_info.copy_num==0)
		wprintw (show_win,"(main copy)");
	wprintw (show_win,"\n");
	refresh_show_win ();
}

void type_ext2_super_block___gocopy (char *command_line)

{
	unsigned long copy_num,offset;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");refresh_command_win ();return;
	}
	ptr=parse_word (ptr,buffer);

	copy_num=atol (buffer);

	offset=file_system_info.super_block_offset+copy_num*file_system_info.no_blocks_in_group*file_system_info.block_size;

	if (offset > file_system_info.file_system_size) {
		wprintw (command_win,"Error - Copy number out of bounds\n");refresh_command_win ();return;
	}

	super_info.copy_num=copy_num;
	device_offset=offset;

	sprintf (buffer,"setoffset %ld",device_offset);dispatch (buffer);
	strcpy (buffer,"show");dispatch (buffer);
}

void type_ext2_super_block___setactivecopy (char *command_line)

{
	struct ext2_super_block sb;

	sb=type_data.u.t_ext2_super_block;
	dispatch ("gocopy 0");
	type_data.u.t_ext2_super_block=sb;
	dispatch ("show");
}

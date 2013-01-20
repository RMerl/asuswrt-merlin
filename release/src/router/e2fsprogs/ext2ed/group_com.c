/*

/usr/src/ext2ed/group_com.c

A part of the extended file system 2 disk editor.

General user commands

First written on: April 17 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"

void type_ext2_group_desc___next (char *command_line)

{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",group_info.group_num+entry_offset);
	dispatch (buffer);
}

void type_ext2_group_desc___prev (char *command_line)

{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",group_info.group_num-entry_offset);
	dispatch (buffer);
}

void type_ext2_group_desc___entry (char *command_line)

{
	long group_num;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");refresh_command_win ();return;
	}
	ptr=parse_word (ptr,buffer);

	group_num=atol (buffer);

	if (group_num < 0 || group_num >= file_system_info.groups_count) {
		wprintw (command_win,"Error - Entry number out of bounds\n");refresh_command_win ();return;
	}

	device_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	sprintf (buffer,"setoffset %ld",device_offset);dispatch (buffer);
	strcpy (buffer,"show");dispatch (buffer);
	group_info.group_num=group_num;
}


void type_ext2_group_desc___gocopy (char *command_line)

{
	unsigned long copy_num,offset;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");refresh_command_win ();return;
	}
	ptr=parse_word (ptr,buffer);

	copy_num=atol (buffer);

	offset=file_system_info.first_group_desc_offset+copy_num*file_system_info.super_block.s_blocks_per_group*file_system_info.block_size;

	if (offset > file_system_info.file_system_size) {
		wprintw (command_win,"Error - Copy number out of bounds\n");refresh_command_win ();return;
	}

	group_info.copy_num=copy_num;
	device_offset=offset+group_info.group_num*sizeof (struct ext2_group_desc);

	sprintf (buffer,"setoffset %ld",device_offset);dispatch (buffer);
	strcpy (buffer,"show");dispatch (buffer);
}


void type_ext2_group_desc___show (char *command_line)

{
	long group_num,temp;

	temp=(device_offset-file_system_info.first_group_desc_offset) % (file_system_info.super_block.s_blocks_per_group*file_system_info.block_size);
	group_num=temp/sizeof (struct ext2_group_desc);

	show (command_line);

	wmove (show_win,1,0);wprintw (show_win,"\n");wmove (show_win,2,0);
	wprintw (show_win,"Group %ld of %ld ",group_num,file_system_info.groups_count-1);
	wprintw (show_win,"in copy %ld ",group_info.copy_num);
	if (group_info.copy_num==0) wprintw (show_win,"(Main copy)");
	wprintw (show_win,"\n");refresh_show_win ();

	if (group_num==0) {
		wprintw (command_win,"Reached first group descriptor\n");
		wrefresh (command_win);
	}

	if (group_num==file_system_info.groups_count-1) {
		wprintw (command_win,"Reached last group descriptor\n");
		wrefresh (command_win);
	}
}

void type_ext2_group_desc___inode (char *command_line)

{
	long inode_offset;
	char buffer [80];

	inode_offset=type_data.u.t_ext2_group_desc.bg_inode_table;
	sprintf (buffer,"setoffset block %ld",inode_offset);dispatch (buffer);
	sprintf (buffer,"settype ext2_inode");dispatch (buffer);
}

void type_ext2_group_desc___blockbitmap (char *command_line)

{
	long block_bitmap_offset;
	char buffer [80];

	block_bitmap_info.entry_num=0;
	block_bitmap_info.group_num=group_info.group_num;

	block_bitmap_offset=type_data.u.t_ext2_group_desc.bg_block_bitmap;
	sprintf (buffer,"setoffset block %ld",block_bitmap_offset);dispatch (buffer);
	sprintf (buffer,"settype block_bitmap");dispatch (buffer);
}

void type_ext2_group_desc___inodebitmap (char *command_line)

{
	long inode_bitmap_offset;
	char buffer [80];

	inode_bitmap_info.entry_num=0;
	inode_bitmap_info.group_num=group_info.group_num;

	inode_bitmap_offset=type_data.u.t_ext2_group_desc.bg_inode_bitmap;
	sprintf (buffer,"setoffset block %ld",inode_bitmap_offset);dispatch (buffer);
	sprintf (buffer,"settype inode_bitmap");dispatch (buffer);
}

void type_ext2_group_desc___setactivecopy (char *command_line)

{
	struct ext2_group_desc gd;

	gd=type_data.u.t_ext2_group_desc;
	dispatch ("gocopy 0");
	type_data.u.t_ext2_group_desc=gd;
	dispatch ("show");
}

/*

/usr/src/ext2ed/inode_com.c

A part of the extended file system 2 disk editor.

Commands relevant to ext2_inode type.

First written on: April 9 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ext2ed.h"

void type_ext2_inode___prev (char *command_line)

{

	char *ptr,buffer [80];

	long group_num,group_offset,entry_num,block_num,first_entry,last_entry;
	long inode_num,mult=1;
	struct ext2_group_desc desc;

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		mult=atol (buffer);
	}

	block_num=device_offset/file_system_info.block_size;

	group_num=inode_offset_to_group_num (device_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	entry_num=(device_offset-desc.bg_inode_table*file_system_info.block_size)/sizeof (struct ext2_inode);

	first_entry=0;last_entry=file_system_info.super_block.s_inodes_per_group-1;
	inode_num=0;

	if (entry_num-mult+1>0) {
		device_offset-=sizeof (struct ext2_inode)*mult;
		entry_num-=mult;

		sprintf (buffer,"setoffset %ld",device_offset);dispatch (buffer);
		strcpy (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Entry out of limits\n");refresh_command_win ();
	}

	if (entry_num==0) {
		wprintw (command_win,"Reached first inode in current group descriptor\n");
		refresh_command_win ();
	}
}

void type_ext2_inode___next (char *command_line)

{

	char *ptr,buffer [80];

	long group_num,group_offset,entry_num,block_num,first_entry,last_entry;
	long inode_num,mult=1;
	struct ext2_group_desc desc;

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		mult=atol (buffer);
	}


	block_num=device_offset/file_system_info.block_size;

	group_num=inode_offset_to_group_num (device_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	entry_num=(device_offset-desc.bg_inode_table*file_system_info.block_size)/sizeof (struct ext2_inode);

	first_entry=0;last_entry=file_system_info.super_block.s_inodes_per_group-1;
	inode_num=0;

	if (entry_num+mult-1<last_entry) {
		device_offset+=sizeof (struct ext2_inode)*mult;
		entry_num+=mult;

		sprintf (buffer,"setoffset %ld",device_offset);dispatch (buffer);
		strcpy (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Entry out of limits\n");refresh_command_win ();
	}

	if (entry_num==last_entry) {
		wprintw (command_win,"Reached last inode in current group descriptor\n");
		refresh_command_win ();
	}
}


void type_ext2_inode___show (char *command_line)

{
	struct ext2_inode *inode_ptr;

	unsigned short temp;
	int i;

	long group_num,group_offset,entry_num,block_num,first_entry,last_entry,inode_num;
	struct ext2_group_desc desc;

	block_num=device_offset/file_system_info.block_size;

	group_num=inode_offset_to_group_num (device_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	entry_num=(device_offset-desc.bg_inode_table*file_system_info.block_size)/sizeof (struct ext2_inode);
	first_entry=0;last_entry=file_system_info.super_block.s_inodes_per_group-1;
	inode_num=group_num*file_system_info.super_block.s_inodes_per_group+1;
	inode_num+=entry_num;

	inode_ptr=&type_data.u.t_ext2_inode;

	show (command_line);

	wmove (show_pad,0,40);wprintw (show_pad,"octal = %06o ",inode_ptr->i_mode);
	for (i=6;i>=0;i-=3) {
		temp=inode_ptr->i_mode & 0x1ff;
		temp=temp >> i;
		if (temp & 4)
			wprintw (show_pad,"r");
		else
			wprintw (show_pad,"-");

		if (temp & 2)
			wprintw (show_pad,"w");
		else
			wprintw (show_pad,"-");

		if (temp & 1)
			wprintw (show_pad,"x");
		else
			wprintw (show_pad,"-");
	}
	wmove (show_pad,3,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_inode.i_atime));
	wmove (show_pad,4,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_inode.i_ctime));
	wmove (show_pad,5,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_inode.i_mtime));
	wmove (show_pad,6,40);wprintw (show_pad,"%s",ctime ((time_t *) &type_data.u.t_ext2_inode.i_dtime));

	wmove (show_pad,10,40);
	temp=inode_ptr->i_flags;

	if (temp & EXT2_SECRM_FL)
		wprintw (show_pad,"s");
	else
		wprintw (show_pad,"-");


	if (temp & EXT2_UNRM_FL)
		wprintw (show_pad,"u");
	else
		wprintw (show_pad,"-");

	if (temp & EXT2_COMPR_FL)
		wprintw (show_pad,"c");
	else
		wprintw (show_pad,"-");

	if (temp & EXT2_SYNC_FL)
		wprintw (show_pad,"S");
	else
		wprintw (show_pad,"-");

	if (temp & EXT2_IMMUTABLE_FL)
		wprintw (show_pad,"i");
	else
		wprintw (show_pad,"-");

	if (temp & EXT2_APPEND_FL)
		wprintw (show_pad,"a");
	else
		wprintw (show_pad,"-");

	if (temp & EXT2_NODUMP_FL)
		wprintw (show_pad,"d");
	else
		wprintw (show_pad,"-");

	refresh_show_pad ();

	wmove (show_win,1,0);

	wprintw (show_win,"Inode %ld of %ld. Entry %ld of %ld in group descriptor %ld.\n"
		,inode_num,file_system_info.super_block.s_inodes_count,entry_num,last_entry,group_num);

	wprintw (show_win,"Inode type: ");

	if (inode_num < EXT2_GOOD_OLD_FIRST_INO) {
		switch (inode_num) {
			case EXT2_BAD_INO:
				wprintw (show_win,"Bad blocks inode - ");
				break;
			case EXT2_ROOT_INO:
				wprintw (show_win,"Root inode - ");
				break;
			case EXT2_ACL_IDX_INO:
				wprintw (show_win,"ACL index inode - ");
				break;
			case EXT2_ACL_DATA_INO:
				wprintw (show_win,"ACL data inode - ");
				break;
			case EXT2_BOOT_LOADER_INO:
				wprintw (show_win,"Boot loader inode - ");
				break;
			case EXT2_UNDEL_DIR_INO:
				wprintw (show_win,"Undelete directory inode - ");
				break;
			default:
				wprintw (show_win,"Reserved inode - ");
				break;
		}
	}
	if (type_data.u.t_ext2_inode.i_mode==0)
		wprintw (show_win,"Free.            ");

	if (S_ISREG (type_data.u.t_ext2_inode.i_mode))
		wprintw (show_win,"File.            ");

	if (S_ISDIR (type_data.u.t_ext2_inode.i_mode))
		wprintw (show_win,"Directory.       ");

	if (S_ISLNK (type_data.u.t_ext2_inode.i_mode)) {
		wprintw (show_win,"Symbolic link.   ");
		wmove (show_pad,12,40);

		if (inode_ptr->i_size <= 60)
			wprintw (show_pad,"-> %s",(char *) &type_data.u.t_ext2_inode.i_block [0]);
		else
			wprintw (show_pad,"Slow symbolic link\n");
		refresh_show_pad ();
	}

	if (S_ISCHR (type_data.u.t_ext2_inode.i_mode))
		wprintw (show_win,"Character device.");

	if (S_ISBLK (type_data.u.t_ext2_inode.i_mode))
		wprintw (show_win,"Block device.    ");

	wprintw (show_win,"\n");refresh_show_win ();

	if (entry_num==last_entry) {
		wprintw (command_win,"Reached last inode in current group descriptor\n");
		refresh_command_win ();
	}

	if (entry_num==first_entry) {
		wprintw (command_win,"Reached first inode in current group descriptor\n");
		refresh_command_win ();
	}

}

void type_ext2_inode___entry (char *command_line)

{
	char *ptr,buffer [80];

	long group_num,group_offset,entry_num,block_num,wanted_entry;
	struct ext2_group_desc desc;

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) return;
	ptr=parse_word (ptr,buffer);
	wanted_entry=atol (buffer);

	block_num=device_offset/file_system_info.block_size;

	group_num=inode_offset_to_group_num (device_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	entry_num=(device_offset-desc.bg_inode_table*file_system_info.block_size)/sizeof (struct ext2_inode);

	if (wanted_entry > entry_num) {
		sprintf (buffer,"next %ld",wanted_entry-entry_num);
		dispatch (buffer);
	}

	else if (wanted_entry < entry_num) {
		sprintf (buffer,"prev %ld",entry_num-wanted_entry);
		dispatch (buffer);
	}
}

void type_ext2_inode___group (char *command_line)

{
	char buffer [80];

	long group_num,group_offset;

	group_num=inode_offset_to_group_num (device_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	sprintf (buffer,"setoffset %ld",group_offset);dispatch (buffer);
	sprintf (buffer,"settype ext2_group_desc");dispatch (buffer);
}

void type_ext2_inode___file (char *command_line)

{
	char buffer [80];

	if (!S_ISREG (type_data.u.t_ext2_inode.i_mode)) {
		wprintw (command_win,"Error - Inode type is not file\n");refresh_command_win ();
		return;
	}

	if (!init_file_info ()) {
		wprintw (command_win,"Error - Unable to show file\n");refresh_command_win ();
		return;
	}

	sprintf (buffer,"settype file");dispatch (buffer);
}

void type_ext2_inode___dir (char *command_line)

{
	char buffer [80];

	if (!S_ISDIR (type_data.u.t_ext2_inode.i_mode)) {
		wprintw (command_win,"Error - Inode type is not directory\n");refresh_command_win ();
		return;
	}

/* It is very important to init first_file_info first, as search_dir_entries relies on it */

	if (!init_dir_info (&first_file_info)) {
		wprintw (command_win,"Error - Unable to show directory\n");refresh_command_win ();
		return;
	}

	file_info=first_file_info;

	sprintf (buffer,"settype dir");dispatch (buffer);
}

long inode_offset_to_group_num (long inode_offset)

{
	int found=0;
	struct ext2_group_desc desc;

	long block_num,group_offset,group_num;

	block_num=inode_offset/file_system_info.block_size;

	group_offset=file_system_info.first_group_desc_offset;
	group_num=(group_offset-file_system_info.first_group_desc_offset)/sizeof (struct ext2_group_desc);

	while (!found && group_num>=0 && group_num<file_system_info.groups_count) {
		low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);
		if (block_num>=desc.bg_inode_table && block_num<desc.bg_inode_table+file_system_info.blocks_per_group)
			found=1;
		else
			group_offset+=sizeof (struct ext2_group_desc);
		group_num=(group_offset-file_system_info.first_group_desc_offset)/sizeof (struct ext2_group_desc);
	}

	if (!found)
		return (-1);

	return (group_num);
}



long int inode_offset_to_inode_num (long inode_offset)

{
	long group_num,group_offset,entry_num,block_num,first_entry,last_entry,inode_num;
	struct ext2_group_desc desc;

	block_num=inode_offset/file_system_info.block_size;

	group_num=inode_offset_to_group_num (inode_offset);
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);

	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	entry_num=(inode_offset-desc.bg_inode_table*file_system_info.block_size)/sizeof (struct ext2_inode);
	first_entry=0;last_entry=file_system_info.super_block.s_inodes_per_group-1;
	inode_num=group_num*file_system_info.super_block.s_inodes_per_group+1;
	inode_num+=entry_num;

	return (inode_num);
}

long int inode_num_to_inode_offset (long inode_num)

{
	long group_num,group_offset,inode_offset,inode_entry;
	struct ext2_group_desc desc;

	inode_num--;

	group_num=inode_num/file_system_info.super_block.s_inodes_per_group;
	inode_entry=inode_num%file_system_info.super_block.s_inodes_per_group;
	group_offset=file_system_info.first_group_desc_offset+group_num*sizeof (struct ext2_group_desc);
	low_read ((char *) &desc,sizeof (struct ext2_group_desc),group_offset);

	inode_offset=desc.bg_inode_table*file_system_info.block_size+inode_entry*sizeof (struct ext2_inode);

	return (inode_offset);
}

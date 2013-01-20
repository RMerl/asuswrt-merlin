/*

/usr/src/ext2ed/file_com.c

A part of the extended file system 2 disk editor.

----------------------------
Commands which handle a file
----------------------------

First written on: April 18 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"

int init_file_info (void)

{
	struct ext2_inode *ptr;

	ptr=&type_data.u.t_ext2_inode;

	file_info.inode_ptr=ptr;
	file_info.inode_offset=device_offset;

	file_info.global_block_num=ptr->i_block [0];
	file_info.global_block_offset=ptr->i_block [0]*file_system_info.block_size;
	file_info.block_num=0;
	file_info.blocks_count=(ptr->i_size+file_system_info.block_size-1)/file_system_info.block_size;
	file_info.file_offset=0;
	file_info.file_length=ptr->i_size;
	file_info.level=0;
	file_info.offset_in_block=0;

	file_info.display=HEX;

	low_read (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);

	return (1);
}


void type_file___inode (char *command_line)

{
	dispatch ("settype ext2_inode");
}

void type_file___show (char *command_line)

{
	if (file_info.display==HEX)
		file_show_hex ();
	if (file_info.display==TEXT)
		file_show_text ();
}

void type_file___nextblock (char *command_line)

{
	long block_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		block_offset*=atol (buffer);
	}

	if (file_info.block_num+block_offset >= file_info.blocks_count) {
		wprintw (command_win,"Error - Block offset out of range\n");wrefresh (command_win);
		return;
	}

	file_info.block_num+=block_offset;
	file_info.global_block_num=file_block_to_global_block (file_info.block_num,&file_info);
	file_info.global_block_offset=file_info.global_block_num*file_system_info.block_size;
	file_info.file_offset=file_info.block_num*file_system_info.block_size;

	low_read (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);

	strcpy (buffer,"show");dispatch (buffer);
}

void type_file___next (char *command_line)

{
	int offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	if (file_info.offset_in_block+offset < file_system_info.block_size) {
		file_info.offset_in_block+=offset;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Offset out of block\n");refresh_command_win ();
	}
}

void type_file___offset (char *command_line)

{
	unsigned long offset;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset=atol (buffer);
	}
	else {
		wprintw (command_win,"Error - Argument not specified\n");refresh_command_win ();
		return;
	}

	if (offset < file_system_info.block_size) {
		file_info.offset_in_block=offset;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Offset out of block\n");refresh_command_win ();
	}
}

void type_file___prev (char *command_line)

{
	int offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	if (file_info.offset_in_block-offset >= 0) {
		file_info.offset_in_block-=offset;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Offset out of block\n");refresh_command_win ();
	}
}

void type_file___prevblock (char *command_line)

{
	long block_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		block_offset*=atol (buffer);
	}

	if (file_info.block_num-block_offset < 0) {
		wprintw (command_win,"Error - Block offset out of range\n");wrefresh (command_win);
		return;
	}

	file_info.block_num-=block_offset;
	file_info.global_block_num=file_block_to_global_block (file_info.block_num,&file_info);
	file_info.global_block_offset=file_info.global_block_num*file_system_info.block_size;
	file_info.file_offset=file_info.block_num*file_system_info.block_size;

	low_read (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);

	strcpy (buffer,"show");dispatch (buffer);
}

void type_file___block (char *command_line)

{
	long block_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr==0) {
		wprintw (command_win,"Error - Invalid arguments\n");wrefresh (command_win);
		return;
	}

	ptr=parse_word (ptr,buffer);
	block_offset=atol (buffer);

	if (block_offset < 0 || block_offset >= file_info.blocks_count) {
		wprintw (command_win,"Error - Block offset out of range\n");wrefresh (command_win);
		return;
	}

	file_info.block_num=block_offset;
	file_info.global_block_num=file_block_to_global_block (file_info.block_num,&file_info);
	file_info.global_block_offset=file_info.global_block_num*file_system_info.block_size;
	file_info.file_offset=file_info.block_num*file_system_info.block_size;

	low_read (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);

	strcpy (buffer,"show");dispatch (buffer);
}

void type_file___display (char *command_line)

{
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr==0)
		strcpy (buffer,"hex");
	else
		ptr=parse_word (ptr,buffer);

	if (strcasecmp (buffer,"hex")==0) {
		wprintw (command_win,"Display set to hex\n");wrefresh (command_win);
		file_info.display=HEX;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else if (strcasecmp (buffer,"text")==0) {
		wprintw (command_win,"Display set to text\n");wrefresh (command_win);
		file_info.display=TEXT;
		sprintf (buffer,"show");dispatch (buffer);
	}

	else {
		wprintw (command_win,"Error - Invalid arguments\n");wrefresh (command_win);
	}
}

void file_show_hex (void)

{
	long offset=0,l,i;
	unsigned char *ch_ptr;

	/* device_offset and type_data points to the inode */

	show_pad_info.line=0;

	wmove (show_pad,0,0);
	ch_ptr=file_info.buffer;
	for (l=0;l<file_system_info.block_size/16;l++) {
		if (file_info.file_offset+offset>file_info.file_length-1) break;
		wprintw (show_pad,"%08ld :  ",offset);
		for (i=0;i<16;i++) {

			if (file_info.file_offset+offset+i>file_info.file_length-1) {
				wprintw (show_pad," ");
			}

			else {
				if (file_info.offset_in_block==offset+i)
					wattrset (show_pad,A_REVERSE);

				if (ch_ptr [i]>=' ' && ch_ptr [i]<='z')
					wprintw (show_pad,"%c",ch_ptr [i]);
				else
					wprintw (show_pad,".");

				if (file_info.offset_in_block==offset+i)
					wattrset (show_pad,A_NORMAL);
			}
		}

		wprintw (show_pad,"   ");
		for (i=0;i<16;i++) {
			if (file_info.file_offset+offset+i>file_info.file_length-1) break;
			if (file_info.offset_in_block==offset+i)
				wattrset (show_pad,A_REVERSE);

			wprintw (show_pad,"%02x",ch_ptr [i]);

			if (file_info.offset_in_block==offset+i) {
				wattrset (show_pad,A_NORMAL);
				show_pad_info.line=l-l % show_pad_info.display_lines;
			}

			wprintw (show_pad," ");

		}

		wprintw (show_pad,"\n");
		offset+=i;
		ch_ptr+=i;
	}

	show_pad_info.max_line=l-1;

	refresh_show_pad ();

	show_status ();
}

void file_show_text (void)

{
	long offset=0,last_offset,l=0,cols=0;
	unsigned char *ch_ptr;

	/* device_offset and type_data points to the inode */

	show_pad_info.line=0;
	wmove (show_pad,0,0);
	ch_ptr=file_info.buffer;

	last_offset=file_system_info.block_size-1;

	if (file_info.file_offset+last_offset > file_info.file_length-1)
		last_offset=file_info.file_length-1-file_info.file_offset;

	while ( (offset <= last_offset) && l<SHOW_PAD_LINES) {

		if (cols==SHOW_PAD_COLS-1) {
			wprintw (show_pad,"\n");
			l++;cols=0;
		}


		if (file_info.offset_in_block==offset)
			wattrset (show_pad,A_REVERSE);

		if (*ch_ptr >= ' ' && *ch_ptr <= 'z')
			wprintw (show_pad,"%c",*ch_ptr);


		else {
			if (*ch_ptr == 0xa) {
				wprintw (show_pad,"\n");
				l++;cols=0;
			}

			else if (*ch_ptr == 0x9)
				wprintw (show_pad,"    ");

			else
				wprintw (show_pad,".");
		}

		if (file_info.offset_in_block==offset) {
			wattrset (show_pad,A_NORMAL);
			show_pad_info.line=l-l % show_pad_info.display_lines;
		}


		offset++;cols++;ch_ptr++;
	}

	wprintw (show_pad,"\n");
	show_pad_info.max_line=l;

	refresh_show_pad ();

	show_status ();
}

void show_status (void)

{
	long inode_num;

	werase (show_win);wmove (show_win,0,0);
	wprintw (show_win,"File contents. Block %ld. ",file_info.global_block_num);
	wprintw (show_win,"File block %ld of %ld. ",file_info.block_num,file_info.blocks_count-1);
	wprintw (show_win,"File Offset %ld of %ld.",file_info.file_offset,file_info.file_length-1);

	wmove (show_win,1,0);
	inode_num=inode_offset_to_inode_num (file_info.inode_offset);
	wprintw (show_win,"File inode %ld. Indirection level %ld.",inode_num,file_info.level);

	refresh_show_win ();
}

void type_file___remember (char *command_line)

{
	int found=0;
	long entry_num;
	char *ptr,buffer [80];
	struct struct_descriptor *descriptor_ptr;

	ptr=parse_word (command_line,buffer);

	if (*ptr==0) {
		wprintw (command_win,"Error - Argument not specified\n");wrefresh (command_win);
		return;
	}

	ptr=parse_word (ptr,buffer);

	entry_num=remember_lifo.entries_count++;
	if (entry_num>REMEMBER_COUNT-1) {
		entry_num=0;
		remember_lifo.entries_count--;
	}

	descriptor_ptr=first_type;
	while (descriptor_ptr!=NULL && !found) {
		if (strcmp (descriptor_ptr->name,"ext2_inode")==0)
			found=1;
		else
			descriptor_ptr=descriptor_ptr->next;
	}


	remember_lifo.offset [entry_num]=device_offset;
	remember_lifo.type [entry_num]=descriptor_ptr;
	strcpy (remember_lifo.name [entry_num],buffer);

	wprintw (command_win,"Object %s in Offset %ld remembered as %s\n",descriptor_ptr->name,device_offset,buffer);
	wrefresh (command_win);
}

void type_file___set (char *command_line)

{
	unsigned char tmp;
	char *ptr,buffer [80],*ch_ptr;
	int mode=HEX;

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - Argument not specified\n");refresh_command_win ();return;
	}

	ptr=parse_word (ptr,buffer);

	if (strcasecmp (buffer,"text")==0) {
		mode=TEXT;
		strcpy (buffer,ptr);
	}

	else if (strcasecmp (buffer,"hex")==0) {
		mode=HEX;
		ptr=parse_word (ptr,buffer);
	}

	if (*buffer==0) {
		wprintw (command_win,"Error - Data not specified\n");refresh_command_win ();return;
	}

	if (mode==HEX) {
		do {
			tmp=(unsigned char) strtol (buffer,NULL,16);
			file_info.buffer [file_info.offset_in_block]=tmp;
			file_info.offset_in_block++;
			ptr=parse_word (ptr,buffer);
			if (file_info.offset_in_block==file_system_info.block_size) {
				if (*ptr) {
					wprintw (command_win,"Error - Ending offset outside block, only partial string changed\n");
					refresh_command_win ();
				}
				file_info.offset_in_block--;
			}
		} while (*buffer) ;
	}

	else {
		ch_ptr=buffer;
		while (*ch_ptr) {
			tmp=(unsigned char) *ch_ptr++;
			file_info.buffer [file_info.offset_in_block]=tmp;
			file_info.offset_in_block++;
			if (file_info.offset_in_block==file_system_info.block_size) {
				if (*ch_ptr) {
					wprintw (command_win,"Error - Ending offset outside block, only partial string changed\n");
					refresh_command_win ();
				}
				file_info.offset_in_block--;
			}
		}
	}

	strcpy (buffer,"show");dispatch (buffer);
}

void type_file___writedata (char *command_line)

{
	low_write (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);
	return;
}

long file_block_to_global_block (long file_block,struct struct_file_info *file_info_ptr)

{
	long last_direct,last_indirect,last_dindirect;

	last_direct=EXT2_NDIR_BLOCKS-1;
	last_indirect=last_direct+file_system_info.block_size/4;
	last_dindirect=last_indirect+(file_system_info.block_size/4)*(file_system_info.block_size/4);

	if (file_block <= last_direct) {
		file_info_ptr->level=0;
		return (file_info_ptr->inode_ptr->i_block [file_block]);
	}

	if (file_block <= last_indirect) {
		file_info_ptr->level=1;
		file_block=file_block-last_direct-1;
		return (return_indirect (file_info_ptr->inode_ptr->i_block [EXT2_IND_BLOCK],file_block));
	}

	if (file_block <= last_dindirect) {
		file_info_ptr->level=2;
		file_block=file_block-last_indirect-1;
		return (return_dindirect (file_info_ptr->inode_ptr->i_block [EXT2_DIND_BLOCK],file_block));
	}

	file_info_ptr->level=3;
	file_block=file_block-last_dindirect-1;
	return (return_tindirect (file_info_ptr->inode_ptr->i_block [EXT2_TIND_BLOCK],file_block));
}

long return_indirect (long table_block,long block_num)

{
	long block_table [EXT2_MAX_BLOCK_SIZE/4];

	low_read ((char *) block_table,file_system_info.block_size,table_block*file_system_info.block_size);
	return (block_table [block_num]);
}

long return_dindirect (long table_block,long block_num)

{
	long f_indirect;

	f_indirect=block_num/(file_system_info.block_size/4);
	f_indirect=return_indirect (table_block,f_indirect);
	return (return_indirect (f_indirect,block_num%(file_system_info.block_size/4)));
}

long return_tindirect (long table_block,long block_num)

{
	long s_indirect;

	s_indirect=block_num/((file_system_info.block_size/4)*(file_system_info.block_size/4));
	s_indirect=return_indirect (table_block,s_indirect);
	return (return_dindirect (s_indirect,block_num%((file_system_info.block_size/4)*(file_system_info.block_size/4))));
}

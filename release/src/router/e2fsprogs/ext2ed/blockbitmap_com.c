/*

/usr/src/ext2ed/blockbitmap_com.c

A part of the extended file system 2 disk editor.

-------------------------
Handles the block bitmap.
-------------------------

This file implements the commands which are specific to the blockbitmap type.

First written on: July 5 1995

Copyright (C) 1995 Gadi Oxman

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"

/*

The functions in this file use the flobal structure block_bitmap_info. This structure contains the current
position in the bitmap.

*/

void type_ext2_block_bitmap___entry (char *command_line)

/*

This function changes the current entry in the bitmap. It just changes the entry_num variable in block_bitmap_info
and dispatches a show command to show the new entry.

*/

{
	unsigned long entry_num;
	char *ptr,buffer [80];



	ptr=parse_word (command_line,buffer);					/* Get the requested entry */
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");
		refresh_command_win ();	return;
	}
	ptr=parse_word (ptr,buffer);

	entry_num=atol (buffer);


	if (entry_num >= file_system_info.super_block.s_blocks_per_group) {	/* Check if it is a valid entry number */

		wprintw (command_win,"Error - Entry number out of bounds\n");
		refresh_command_win ();return;
	}



	block_bitmap_info.entry_num=entry_num;					/* If it is, just change entry_num and */
	strcpy (buffer,"show");dispatch (buffer);				/* dispatch a show command */
}

void type_ext2_block_bitmap___next (char *command_line)

/*

This function passes to the next entry in the bitmap. We just call the above entry command.

*/

{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",block_bitmap_info.entry_num+entry_offset);
	dispatch (buffer);
}

void type_ext2_block_bitmap___prev (char *command_line)

{
	long entry_offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		entry_offset=atol (buffer);
	}

	sprintf (buffer,"entry %ld",block_bitmap_info.entry_num-entry_offset);
	dispatch (buffer);
}

void type_ext2_block_bitmap___allocate (char *command_line)

/*

This function starts allocating block from the current position. Allocating involves setting the correct bits
in the bitmap. This function is a vector version of allocate_block below - We just run on the blocks that
we need to allocate, and call allocate_block for each one.

*/

{
	long entry_num,num=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);					/* Get the number of blocks to allocate */
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		num=atol (buffer);
	}

	entry_num=block_bitmap_info.entry_num;
										/* Check for limits */
	if (num > file_system_info.super_block.s_blocks_per_group-entry_num) {
		wprintw (command_win,"Error - There aren't that much blocks in the group\n");
		refresh_command_win ();return;
	}

	while (num) {								/* And call allocate_block */
		allocate_block (entry_num);					/* for each block */
		num--;entry_num++;
	}

	dispatch ("show");							/* Show the result */
}

void type_ext2_block_bitmap___deallocate (char *command_line)

/* This is the opposite of the above function - We call deallocate_block instead of allocate_block */

{
	long entry_num,num=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		num=atol (buffer);
	}

	entry_num=block_bitmap_info.entry_num;
	if (num > file_system_info.super_block.s_blocks_per_group-entry_num) {
		wprintw (command_win,"Error - There aren't that much blocks in the group\n");
		refresh_command_win ();return;
	}

	while (num) {
		deallocate_block (entry_num);
		num--;entry_num++;
	}

	dispatch ("show");
}


void allocate_block (long entry_num)

/* In this function we convert the bit number into the right byte and inner bit positions. */

{
	unsigned char bit_mask=1;
	int byte_offset,j;

	byte_offset=entry_num/8;					/* Find the correct byte - entry_num/8 */
									/* The position inside the byte is entry_num %8 */
	for (j=0;j<entry_num%8;j++)
		bit_mask*=2;						/* Generate the or mask - 1 at the right place */
	type_data.u.buffer [byte_offset] |= bit_mask;			/* And apply it */
}

void deallocate_block (long entry_num)

/* This is the opposite of allocate_block above. We use an and mask instead of an or mask. */

{
	unsigned char bit_mask=1;
	int byte_offset,j;

	byte_offset=entry_num/8;
	for (j=0;j<entry_num%8;j++)
		bit_mask*=2;
	bit_mask^=0xff;

	type_data.u.buffer [byte_offset] &= bit_mask;
}

void type_ext2_block_bitmap___show (char *command_line)

/*

We show the bitmap as a series of bits, grouped at 8-bit intervals. We display 8 such groups on each line.
The current position (as known from block_bitmap_info.entry_num) is highlighted.

*/

{
	int i,j;
	unsigned char *ptr;
	unsigned long block_num,entry_num;

	ptr=type_data.u.buffer;
	show_pad_info.line=0;show_pad_info.max_line=-1;

	wmove (show_pad,0,0);
	for (i=0,entry_num=0;i<file_system_info.super_block.s_blocks_per_group/8;i++,ptr++) {
		for (j=1;j<=128;j*=2) {						/* j contains the and bit mask */
			if (entry_num==block_bitmap_info.entry_num) {		/* Highlight the current entry */
				wattrset (show_pad,A_REVERSE);
				show_pad_info.line=show_pad_info.max_line-show_pad_info.display_lines/2;
			}

			if ((*ptr) & j)						/* Apply the mask */
				wprintw (show_pad,"1");
			else
				wprintw (show_pad,"0");

			if (entry_num==block_bitmap_info.entry_num)
				wattrset (show_pad,A_NORMAL);

			entry_num++;						/* Pass to the next entry */
		}
		wprintw (show_pad," ");
		if (i%8==7) {							/* Display 8 groups in a row */
			wprintw (show_pad,"\n");
			show_pad_info.max_line++;
		}
	}

	refresh_show_pad ();
	show_info ();								/* Show the usual information */

										/* Show the group number */
	wmove (show_win,1,0);
	wprintw (show_win,"Block bitmap of block group %ld\n",block_bitmap_info.group_num);
										/* Show the block number */

	block_num=block_bitmap_info.entry_num+block_bitmap_info.group_num*file_system_info.super_block.s_blocks_per_group;
	block_num+=file_system_info.super_block.s_first_data_block;

	wprintw (show_win,"Status of block %ld - ",block_num);			/* and the allocation status */
	ptr=type_data.u.buffer+block_bitmap_info.entry_num/8;
	j=1;
	for (i=block_bitmap_info.entry_num % 8;i>0;i--)
		j*=2;
	if ((*ptr) & j)
		wprintw (show_win,"Allocated\n");
	else
		wprintw (show_win,"Free\n");
	refresh_show_win ();
}

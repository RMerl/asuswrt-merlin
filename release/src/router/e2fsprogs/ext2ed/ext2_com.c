/*

/usr/src/ext2ed/ext2_com.c

A part of the extended file system 2 disk editor.

--------------------------------------
Extended-2 filesystem General commands
--------------------------------------

The commands here will be registered when we are editing an ext2 filesystem

First written on: July 28 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"

void type_ext2___super (char *command_line)

/*

We are moving to the superblock - Just use setoffset and settype. The offset was gathered in the
initialization phase (but is constant - 1024).

*/

{
	char buffer [80];

	super_info.copy_num=0;
	sprintf (buffer,"setoffset %ld",file_system_info.super_block_offset);dispatch (buffer);
	sprintf (buffer,"settype ext2_super_block");dispatch (buffer);
}

void type_ext2___cd (char *command_line)

/*

A global cd command - The path should start with /.

We implement it through dispatching to our primitive functions.

*/

{
	char temp [80],buffer [80],*ptr;

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - No argument specified\n");refresh_command_win ();return;
	}
	ptr=parse_word (ptr,buffer);

	if (buffer [0] != '/') {
		wprintw (command_win,"Error - Use a full pathname (begin with '/')\n");refresh_command_win ();return;
	}

	/* Note the various dispatches below - They should be intuitive if you know the ext2 filesystem structure */

	dispatch ("super");dispatch ("group");dispatch ("inode");dispatch ("next");dispatch ("dir");
	if (buffer [1] != 0) {
		sprintf (temp,"cd %s",buffer+1);dispatch (temp);
	}
}

void type_ext2___group (char *command_line)

/*

We go to the group descriptors.
First, we go to the first group descriptor in the main copy.
Then, we use the group's entry command to pass to another group.

*/

{
	long group_num=0;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		group_num=atol (buffer);
	}

	group_info.copy_num=0;group_info.group_num=0;
	sprintf (buffer,"setoffset %ld",file_system_info.first_group_desc_offset);dispatch (buffer);
	sprintf (buffer,"settype ext2_group_desc");dispatch (buffer);
	sprintf (buffer,"entry %ld",group_num);dispatch (buffer);
}

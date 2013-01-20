/*

/usr/src/ext2ed/dir_com.c

A part of the extended file system 2 disk editor.

--------------------
Handles directories.
--------------------

This file contains the codes which allows the user to handle directories.

Most of the functions use the global variable file_info (along with the special directory fields there) to save
information and pass it between them.

Since a directory is just a big file which is composed of directory entries, you will find that
the functions here are a superset of those in the file_com.c source.

We assume that the user reached here using the dir command of the inode type and not by using settype dir, so
that init_dir_info is indeed called to gather the required information.

type_data is not changed! It still contains the inode of the file - We handle the directory in our own
variables, so that settype ext2_inode will "go back" to the inode of this directory.

First written on: April 28 1995

Copyright (C) 1995 Gadi Oxman

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ext2ed.h"

char name_search [80];
long entry_num_search;

int init_dir_info (struct struct_file_info *info_ptr)

/*

This function is called by the inode of the directory when the user issues the dir command from the inode.
It is used to gather information about the inode and to reset some variables which we need in order to handle
directories.

*/

{
	struct ext2_inode *ptr;

	ptr=&type_data.u.t_ext2_inode;					/* type_data contains the inode */

	info_ptr->inode_ptr=ptr;
	info_ptr->inode_offset=device_offset;				/* device offset contains the inode's offset */

									/* Reset the current position to the start */

	info_ptr->global_block_num=ptr->i_block [0];
	info_ptr->global_block_offset=ptr->i_block [0]*file_system_info.block_size;
	info_ptr->block_num=0;
	info_ptr->file_offset=0;
									/* Set the size of the directory */

	info_ptr->blocks_count=(ptr->i_size+file_system_info.block_size-1)/file_system_info.block_size;
	info_ptr->file_length=ptr->i_size;

	info_ptr->level=0;						/* We start using direct blocks */
	info_ptr->display=HEX;						/* This is not actually used */

	info_ptr->dir_entry_num=0;info_ptr->dir_entries_count=0;	/* We'll start at the first directory entry */
	info_ptr->dir_entry_offset=0;

	/* Find dir_entries_count */

	info_ptr->dir_entries_count=count_dir_entries (); 		/* Set the total number of entries */

	return (1);
}

struct struct_file_info search_dir_entries (int (*action) (struct struct_file_info *info),int *status)

/*
	This is the main function in this source file. Various actions are implemented using this basic function.

	This routine runs on all directory entries in the current directory.
	For each entry, action is called. We'll act according to the return code of action:

		ABORT		-	Current dir entry is returned.
		CONTINUE	-	Continue searching.
		FOUND		-	Current dir entry is returned.

	If the last entry is reached, it is returned, along with an ABORT status.

	status is updated to the returned code of action.
*/

{
	struct struct_file_info info;						/* Temporary variables used to */
	struct ext2_dir_entry_2 *dir_entry_ptr;					/* contain the current search entries */
	int return_code, next;

	info=first_file_info;							/* Start from the first entry - Read it */
	low_read (info.buffer,file_system_info.block_size,info.global_block_offset);
	dir_entry_ptr=(struct ext2_dir_entry_2 *) (info.buffer+info.dir_entry_offset);

	while (info.file_offset < info.file_length) {				/* While we haven't reached the end */

		*status=return_code=action (&info);				/* Call the client function to test */
										/* the current entry */
		if (return_code==ABORT || return_code==FOUND)
			return (info);						/* Stop, if so asked */

										/* Pass to the next entry */

		dir_entry_ptr=(struct ext2_dir_entry_2 *) (info.buffer+info.dir_entry_offset);

		info.dir_entry_num++;
		next = dir_entry_ptr->rec_len;
		if (!next)
			next = file_system_info.block_size - info.dir_entry_offset;
		info.dir_entry_offset += next;
		info.file_offset += next;

		if (info.file_offset >= info.file_length) break;

		if (info.dir_entry_offset >= file_system_info.block_size) {	/* We crossed a block boundary */
										/* Find the next block, */
			info.block_num++;
			info.global_block_num=file_block_to_global_block (info.block_num,&info);
			info.global_block_offset=info.global_block_num*file_system_info.block_size;
			info.file_offset=info.block_num*file_system_info.block_size;
			info.dir_entry_offset=0;
										/* read it and update the pointer */

			low_read (info.buffer,file_system_info.block_size,info.global_block_offset);
			dir_entry_ptr=(struct ext2_dir_entry_2 *) (info.buffer+info.dir_entry_offset);

		}

	}

	*status=ABORT;return (info);						/* There was no match */
}

long count_dir_entries (void)

/*

This function counts the number of entries in the directory. We just call search_dir_entries till the end.
The client function is action_count, which just tell search_dir_entries to continue.

*/

{
	int status;

	return (search_dir_entries (&action_count,&status).dir_entry_num);
}

int action_count (struct struct_file_info *info)

/*

Used by count_dir_entries above - This function is called by search_dir_entries, and it tells it to continue
searching, until we get to the last entry.

*/

{
	return (CONTINUE);							/* Just continue searching */
}

void type_dir___cd (char *command_line)

/*
	Changes to a directory, relative to the current directory.

	This is a complicated operation, so I would repeat here the explanation from the design and
	implementation document.

1.	The path is checked that it is not an absolute path (from /). If it is, we let the general cd to do the job by
	calling directly type_ext2___cd.

2.	The path is divided into the nearest path and the rest of the path. For example, cd 1/2/3/4 is divided into
	1 and into 2/3/4.

3.	It is the first part of the path that we need to search for in the current directory. We search for it using
	search_dir_entries, which accepts the action_name function as the client function.

4.	search_dir_entries will scan the entire entries and will call our action_name function for each entry.
	In action_name, the required name will be checked against the name of the current entry, and FOUND will be
	returned when a match occurs.

5.	If the required entry is found, we dispatch a remember command to insert the current inode (remember that
	type_data is still intact and contains the inode of the current directory) into the object memory.
	This is required to easily support symbolic links - If we find later that the inode pointed by the entry is
	actually a symbolic link, we'll need to return to this point, and the above inode doesn't have (and can't have,
	because of hard links) the information necessary to "move back".

6.	We then dispatch a followinode command to reach the inode pointed by the required entry. This command will
	automatically change the type to ext2_inode - We are now at an inode, and all the inode commands are available.

7.	We check the inode's type to see if it is a directory. If it is, we dispatch a dir command to "enter the directory",
	and recursively call ourself (The type is dir again) by dispatching a cd command, with the rest of the path
	as an argument.

8.	If the inode's type is a symbolic link (only fast symbolic link were meanwhile implemented. I guess this is
	typically the case.), we note the path it is pointing at, the saved inode is recalled, we dispatch dir to
	get back to the original directory, and we call ourself again with the link path/rest of the path argument.

9.	In any other case, we just stop at the resulting inode.

*/

{
	int status;
	char *ptr,full_dir_name [500],dir_name [500],temp [500],temp2 [500];
	struct struct_file_info info;
	struct ext2_dir_entry_2 *dir_entry_ptr;

	dir_entry_ptr=(struct ext2_dir_entry_2 *) (file_info.buffer+file_info.dir_entry_offset);

	ptr=parse_word (command_line,dir_name);

	if (*ptr==0) {						/* cd alone will enter the highlighted directory */
		strncpy (full_dir_name,dir_entry_ptr->name,dir_entry_ptr->name_len);
		full_dir_name [dir_entry_ptr->name_len]=0;
	}
	else
		ptr=parse_word (ptr,full_dir_name);

	ptr=strchr (full_dir_name,'/');

	if (ptr==full_dir_name) {				/* Pathname is from root - Let the general cd do the job */
		sprintf (temp,"cd %s",full_dir_name);type_ext2___cd (temp);return;
	}

	if (ptr==NULL) {
		strcpy (dir_name,full_dir_name);
		full_dir_name [0]=0;
	}

	else {
		strncpy (dir_name,full_dir_name,ptr-full_dir_name);
		dir_name [ptr-full_dir_name]=0;
		strcpy (full_dir_name,++ptr);
	}
								/* dir_name contains the current entry, while */
								/* full_dir_name contains the rest */

	strcpy (name_search,dir_name);				/* name_search is used to hold the required entry name */

	if (dir_entry_ptr->name_len != strlen (dir_name) ||
	    strncmp (dir_name,dir_entry_ptr->name,dir_entry_ptr->name_len)!=0)
		info=search_dir_entries (&action_name,&status);	/* Search for the entry. Answer in info. */
	else {
		status=FOUND;info=file_info;
	}

	if (status==FOUND) {					/* If found */
		file_info=info;					/* Switch to it, by setting the global file_info */
		dispatch ("remember internal_variable");	/* Move the inode into the objects memory */

		dispatch ("followinode");			/* Go to the inode pointed by this directory entry */

		if (S_ISLNK (type_data.u.t_ext2_inode.i_mode)) {/* Symbolic link ? */

			if (type_data.u.t_ext2_inode.i_size > 60) {	/* I'm lazy, I guess :-) */
				wprintw (command_win,"Error - Sorry, Only fast symbolic link following is currently supported\n");
				refresh_command_win ();
				return;
			}
								/* Get the pointed name and append the previous path */

			strcpy (temp2,(unsigned char *) &type_data.u.t_ext2_inode.i_block);
			strcat (temp2,"/");
			strcat (temp2,full_dir_name);

			dispatch ("recall internal_variable");	/* Return to the original inode */
			dispatch ("dir");			/* and to the directory */

			sprintf (temp,"cd %s",temp2);		/* And continue from there by dispatching a cd command */
			dispatch (temp);			/* (which can call ourself or the general cd) */

			return;
		}

		if (S_ISDIR (type_data.u.t_ext2_inode.i_mode)) { /* Is it an inode of a directory ? */

			dispatch ("dir");			/* Yes - Pass to the pointed directory */

			if (full_dir_name [0] != 0) {		/* And call ourself with the rest of the pathname */
				sprintf (temp,"cd %s",full_dir_name);
				dispatch (temp);
			}

			return;
		}

		else {						/* If we can't continue from here, we'll just stop */
			wprintw (command_win,"Can\'t continue - Stopping at last inode\n");refresh_command_win ();
			return;
		}
	}

	wprintw (command_win,"Error - Directory entry %s not found.\n",dir_name);	/* Hmm, an invalid path somewhere */
	refresh_command_win ();
}

int action_name (struct struct_file_info *info)

/*

Compares the current search entry name (somewhere inside info) with the required name (in name_search).
Returns FOUND if found, or CONTINUE if not found.

*/

{
	struct ext2_dir_entry_2 *dir_entry_ptr;

	dir_entry_ptr=(struct ext2_dir_entry_2 *) (info->buffer+info->dir_entry_offset);

	if (dir_entry_ptr->name_len != strlen (name_search))
		return (CONTINUE);

	if (strncmp (dir_entry_ptr->name,name_search,dir_entry_ptr->name_len)==0)
		return (FOUND);

	return (CONTINUE);
}

void type_dir___entry (char *command_line)

/*

Selects a directory entry according to its number.
search_dir_entries is used along with action_entry_num, in the same fashion as the previous usage of search_dir_entries.

*/

{
	int status;
	struct struct_file_info info;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - Argument_not_specified\n");wrefresh (command_win);
		return;
	}
	ptr=parse_word (ptr,buffer);
	entry_num_search=atol (buffer);

	if (entry_num_search < 0 || entry_num_search >= file_info.dir_entries_count) {
		wprintw (command_win,"Error - Entry number out of range\n");wrefresh (command_win);
		return;
	}

	info=search_dir_entries (&action_entry_num,&status);
	if (status==FOUND) {
		file_info=info;
		dispatch ("show");
		return;
	}
#ifdef DEBUG
	internal_error ("dir_com","type_dir___entry","According to our gathered data, we should have found this entry");
#endif
}

int action_entry_num (struct struct_file_info *info)

/*

Used by the above function. Just compares the current number (in info) with the required one.

*/

{
	if (info->dir_entry_num == entry_num_search)
		return (FOUND);

	return (CONTINUE);
}

void type_dir___followinode (char *command_line)

/*

Here we pass to the inode pointed by the current entry.
It involves computing the device offset of the inode and using directly the setoffset and settype commands.

*/
{
	long inode_offset;
	char buffer [80];

	struct ext2_dir_entry_2 *dir_entry_ptr;

	low_read (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);
	dir_entry_ptr=(struct ext2_dir_entry_2 *) (file_info.buffer+file_info.dir_entry_offset);

	inode_offset=inode_num_to_inode_offset (dir_entry_ptr->inode);			/* Compute the inode's offset */
	sprintf (buffer,"setoffset %ld",inode_offset);dispatch (buffer);		/* Move to it */
	sprintf (buffer,"settype ext2_inode");dispatch (buffer);			/* and set the type to an inode */
}

void type_dir___inode (char *command_line)

/*

Returns to the parent inode of the current directory.
This is trivial, as we type_data is still intact and contains the parent inode !

*/

{
	dispatch ("settype ext2_inode");
}


void type_dir___show (char *command_line)

/*

We use search_dir_entries to run on all the entries. Each time, action_show will be called to show one entry.

*/

{
	int status;

	wmove (show_pad,0,0);
	show_pad_info.max_line=-1;

	search_dir_entries (&action_show,&status);
	show_pad_info.line=file_info.dir_entry_num-show_pad_info.display_lines/2;
	refresh_show_pad ();
	show_dir_status ();
}

int action_show (struct struct_file_info *info)

/*

Show the current search entry (info) in one line. If the entry happens to be the current edited entry, it is highlighted.

*/

{
	unsigned char temp [80];
	struct ext2_dir_entry_2 *dir_entry_ptr;

	dir_entry_ptr=(struct ext2_dir_entry_2 *) (info->buffer+info->dir_entry_offset);

	if (info->dir_entry_num == file_info.dir_entry_num)				/* Highlight the current entry */
		wattrset (show_pad,A_REVERSE);

	strncpy (temp,dir_entry_ptr->name,dir_entry_ptr->name_len);			/* The name is not terminated */
	temp [dir_entry_ptr->name_len]=0;
	if (dir_entry_ptr->name_len > (COLS - 55) && COLS > 55)
		temp [COLS-55]=0;
	wprintw (show_pad,"inode = %-8lu rec_len = %-4lu name_len = %-3lu name = %s\n",	/* Display the various fields */
		 dir_entry_ptr->inode,dir_entry_ptr->rec_len,dir_entry_ptr->name_len,temp);

	show_pad_info.max_line++;

	if (info->dir_entry_num == file_info.dir_entry_num)
		wattrset (show_pad,A_NORMAL);

	return (CONTINUE);								/* And pass to the next */
}

void type_dir___next (char *command_line)

/*

This function moves to the next directory entry. It just uses the current information and the entry command.

*/

{
	int offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	sprintf (buffer,"entry %ld",file_info.dir_entry_num+offset);dispatch (buffer);

}

void type_dir___prev (char *command_line)

{
	int offset=1;
	char *ptr,buffer [80];

	ptr=parse_word (command_line,buffer);

	if (*ptr!=0) {
		ptr=parse_word (ptr,buffer);
		offset*=atol (buffer);
	}

	sprintf (buffer,"entry %ld",file_info.dir_entry_num-offset);dispatch (buffer);
}

void show_dir_status (void)

/*

Various statistics about the directory.

*/

{
	long inode_num;

	wmove (show_win,0,0);
	wprintw (show_win,"Directory listing. Block %ld. ",file_info.global_block_num);
	wprintw (show_win,"Directory entry %ld of %ld.\n",file_info.dir_entry_num,file_info.dir_entries_count-1);
	wprintw (show_win,"Directory Offset %ld of %ld. ",file_info.file_offset,file_info.file_length-1);

	inode_num=inode_offset_to_inode_num (file_info.inode_offset);
	wprintw (show_win,"File inode %ld. Indirection level %ld.\n",inode_num,file_info.level);

	refresh_show_win ();
}

void type_dir___remember (char *command_line)

/*

This is overrided here because we don't remember a directory - It is too complicated. Instead, we remember the
inode of the current directory.

*/

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

void type_dir___set (char *command_line)

/*

Since the dir object doesn't have variables, we provide the impression that it has here. ext2_dir_entry was not used
because it is of variable length.

*/

{
	int found=0;
	unsigned char *ptr,buffer [80],variable [80],value [80],temp [80];
	struct ext2_dir_entry_2 *dir_entry_ptr;

	dir_entry_ptr=(struct ext2_dir_entry_2 *) (file_info.buffer+file_info.dir_entry_offset);

	ptr=parse_word (command_line,buffer);
	if (*ptr==0) {
		wprintw (command_win,"Error - Missing arguments\n");refresh_command_win ();
		return;
	}
	parse_word (ptr,buffer);
	ptr=strchr (buffer,'=');
	if (ptr==NULL) {
		wprintw (command_win,"Error - Bad syntax\n");refresh_command_win ();return;
	}
	strncpy (variable,buffer,ptr-buffer);variable [ptr-buffer]=0;
	strcpy (value,++ptr);

	if (strcasecmp ("inode",variable)==0) {
		found=1;
		dir_entry_ptr->inode=atol (value);
		wprintw (command_win,"Variable %s set to %lu\n",variable,dir_entry_ptr->inode);refresh_command_win ();

	}

	if (strcasecmp ("rec_len",variable)==0) {
		found=1;
		dir_entry_ptr->rec_len=(unsigned int) atol (value);
		wprintw (command_win,"Variable %s set to %lu\n",variable,dir_entry_ptr->rec_len);refresh_command_win ();

	}

	if (strcasecmp ("name_len",variable)==0) {
		found=1;
		dir_entry_ptr->name_len=(unsigned int) atol (value);
		wprintw (command_win,"Variable %s set to %lu\n",variable,dir_entry_ptr->name_len);refresh_command_win ();

	}

	if (strcasecmp ("name",variable)==0) {
		found=1;
		if (strlen (value) > dir_entry_ptr->name_len) {
			wprintw (command_win,"Error - Length of name greater then name_len\n");
			refresh_command_win ();return;
		}
		strncpy (dir_entry_ptr->name,value,strlen (value));
		wprintw (command_win,"Variable %s set to %s\n",variable,value);refresh_command_win ();

	}

	if (found) {
		wattrset (show_pad,A_REVERSE);
		strncpy (temp,dir_entry_ptr->name,dir_entry_ptr->name_len);
		temp [dir_entry_ptr->name_len]=0;
		wmove (show_pad,file_info.dir_entry_num,0);
		wprintw (show_pad,"inode = %-8lu rec_len = %-4lu name_len = %-3lu name = %s\n",
			 dir_entry_ptr->inode,dir_entry_ptr->rec_len,dir_entry_ptr->name_len,temp);
		wattrset (show_pad,A_NORMAL);
		show_pad_info.line=file_info.dir_entry_num-show_pad_info.display_lines/2;
		refresh_show_pad ();
		show_dir_status ();
	}

	else {
		wprintw (command_win,"Error - Variable %s not found\n",variable);
		refresh_command_win ();
	}

}

void type_dir___writedata (char *command_line)

/*

We need to override this since the data is not in type_data. Instead, we have to write the buffer which corresponds
to the current block.

*/

{
	low_write (file_info.buffer,file_system_info.block_size,file_info.global_block_offset);
	return;
}

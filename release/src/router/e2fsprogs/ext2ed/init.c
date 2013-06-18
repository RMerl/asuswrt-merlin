/*

/usr/src/ext2ed/init.c

A part of the extended file system 2 disk editor.

--------------------------------
Various initialization routines.
--------------------------------

First written on: April 9 1995

Copyright (C) 1995 Gadi Oxman

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_READLINE
#include <readline.h>
#endif
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ext2ed.h"

char lines_s [80],cols_s [80];

void signal_handler (void);

void prepare_to_close (void)

{
	close_windows ();
	if (device_handle!=NULL)
		fclose (device_handle);
	free_user_commands (&general_commands);
	free_user_commands (&ext2_commands);
	free_struct_descriptors ();
}

int init (void)

{
	printf ("Initializing ...\n");

	if (!process_configuration_file ()) {
		fprintf (stderr,"Error - Unable to complete configuration. Quitting.\n");
		return (0);
	};

	general_commands.last_command=-1;	/* No commands whatsoever meanwhile */
	ext2_commands.last_command=-1;
	add_general_commands ();		/* Add the general commands, aviable always */
	device_handle=NULL;			/* Notice that our device is still not set up */
	device_offset=-1;
	current_type=NULL;			/* No filesystem specific types yet */

	remember_lifo.entries_count=0;		/* Object memory is empty */

	init_windows ();			/* Initialize the NCURSES interface */
	init_readline ();			/* Initialize the READLINE interface */
	init_signals ();			/* Initialize the signal handlers */
	write_access=0;				/* Write access disabled */

	strcpy (last_command_line,"help");	/* Show the help screen to the user */
	dispatch ("help");
	return (1);				/* Success */
}

void add_general_commands (void)

{
	add_user_command (&general_commands,"help","EXT2ED help system",help);
	add_user_command (&general_commands,"set","Changes a variable in the current object",set);
	add_user_command (&general_commands,"setdevice","Selects the filesystem block device (e.g. /dev/hda1)",set_device);
	add_user_command (&general_commands,"setoffset","Moves asynchronicly in the filesystem",set_offset);
	add_user_command (&general_commands,"settype","Tells EXT2ED how to interpert the current object",set_type);
	add_user_command (&general_commands,"show","Displays the current object",show);
	add_user_command (&general_commands,"pgup","Scrolls data one page up",pgup);
	add_user_command (&general_commands,"pgdn","Scrolls data one page down",pgdn);
	add_user_command (&general_commands,"redraw","Redisplay the screen",redraw);
	add_user_command (&general_commands,"remember","Saves the current position and data information",remember);
	add_user_command (&general_commands,"recall","Gets back to the saved object position",recall);
	add_user_command (&general_commands,"enablewrite","Enters Read/Write mode - Allows changing the filesystem",enable_write);
	add_user_command (&general_commands,"disablewrite","Enters read only mode",disable_write);
	add_user_command (&general_commands,"writedata","Write data back to disk",write_data);
	add_user_command (&general_commands,"next","Moves to the next byte in hex mode",next);
	add_user_command (&general_commands,"prev","Moves to the previous byte in hex mode",prev);
}

void add_ext2_general_commands (void)

{
	add_user_command (&ext2_commands,"super","Moves to the superblock of the filesystem",type_ext2___super);
	add_user_command (&ext2_commands,"group","Moves to the first group descriptor",type_ext2___group);
	add_user_command (&ext2_commands,"cd","Moves to the directory specified",type_ext2___cd);
}

int set_struct_descriptors (char *file_name)

{
	FILE *fp;
	char current_line [500],current_word [50],*ch;
	char variable_name [50],variable_type [20];
	struct struct_descriptor *current_descriptor;

	if ( (fp=fopen (file_name,"rt"))==NULL) {
		wprintw (command_win,"Error - Failed to open descriptors file %s\n",file_name);
		refresh_command_win ();	return (0);
	};

	while (!feof (fp)) {
		fgets (current_line,500,fp);
		if (feof (fp)) break;
		ch=parse_word (current_line,current_word);
		if (strcmp (current_word,"struct")==0) {
			ch=parse_word (ch,current_word);
			current_descriptor=add_new_descriptor (current_word);

			while (strchr (current_line,'{')==NULL) {
				fgets (current_line,500,fp);
				if (feof (fp)) break;
			};
			if (feof (fp)) break;

			fgets (current_line,500,fp);

			while (strchr (current_line,'}')==NULL) {
				while (strchr (current_line,';')==NULL) {
					fgets (current_line,500,fp);
					if (strchr (current_line,'}')!=NULL) break;
				};
				if (strchr (current_line,'}') !=NULL) break;
				ch=parse_word (current_line,variable_type);
				ch=parse_word (ch,variable_name);
				while (variable_name [strlen (variable_name)-1]!=';') {
					strcpy (variable_type,variable_name);
					ch=parse_word (ch,variable_name);
				};
				variable_name [strlen (variable_name)-1]=0;
				add_new_variable (current_descriptor,variable_type,variable_name);
				fgets (current_line,500,fp);
			};
		};
	};

	fclose (fp);
	return (1);
}

void free_struct_descriptors (void)

{
	struct struct_descriptor *ptr,*next;

	ptr=first_type;
	while (ptr!=NULL) {
		next=ptr->next;
		free_user_commands (&ptr->type_commands);
		free (ptr);
		ptr=next;
	}
	first_type=last_type=current_type=NULL;
}

void free_user_commands (struct struct_commands *ptr)

{
	int i;

	for (i=0;i<=ptr->last_command;i++) {
		free (ptr->names [i]);
		free (ptr->descriptions [i]);
	}

	ptr->last_command=-1;
}

struct struct_descriptor *add_new_descriptor (char *name)

{
	struct struct_descriptor *ptr;

	ptr = malloc (sizeof (struct struct_descriptor));
	if (ptr == NULL) {
		printf ("Error - Can not allocate memory - Quitting\n");
		exit (1);
	}
	memset(ptr, 0, sizeof(struct struct_descriptor));
	ptr->prev = ptr->next = NULL;
	strcpy (ptr->name,name);
	ptr->length=0;
	ptr->fields_num=0;
	if (first_type==NULL) {
		first_type = last_type = ptr;
	} else {
		ptr->prev = last_type; last_type->next = ptr; last_type=ptr;
	}
	ptr->type_commands.last_command=-1;
	fill_type_commands (ptr);
	return (ptr);
}

struct type_table {
	char 	*name;
	int	field_type;
	int	len;
};

struct type_table type_table[] = {
	{ "long",   FIELD_TYPE_INT,	4 },
	{ "short",  FIELD_TYPE_INT,	2 },
	{ "char",   FIELD_TYPE_CHAR,	1 },
	{ "__u32",  FIELD_TYPE_UINT,	4 },
	{ "__s32",  FIELD_TYPE_INT,	4 },
	{ "__u16",  FIELD_TYPE_UINT,	2 },
	{ "__s16",  FIELD_TYPE_INT,	2 },
	{ "__u8",   FIELD_TYPE_UINT,	1 },
	{ "__s8",   FIELD_TYPE_INT,	1 },
	{  0,       0,                  0 }
};

void add_new_variable (struct struct_descriptor *ptr,char *v_type,char *v_name)

{
	short	len=1;
	char	field_type=FIELD_TYPE_INT;
	struct type_table *p;

	strcpy (ptr->field_names [ptr->fields_num],v_name);
	ptr->field_positions [ptr->fields_num]=ptr->length;

	for (p = type_table; p->name; p++) {
		if (strcmp(v_type, p->name) == 0) {
			len = p->len;
			field_type = p->field_type;
			break;
		}
	}
	if (p->name == 0) {
		if (strncmp(v_type, "char[", 5) == 0) {
			len = atoi(v_type+5);
			field_type = FIELD_TYPE_CHAR;
		} else {
			printf("Unknown type %s for field %s\n", v_type, v_name);
			exit(1);
		}
	}

	ptr->field_lengths [ptr->fields_num] = len;
	ptr->field_types [ptr->fields_num] = field_type;

	ptr->length+=len;
	ptr->fields_num++;
}

void fill_type_commands (struct struct_descriptor *ptr)

/*

Set specific type user commands.

*/

{

	if (strcmp ((ptr->name),"file")==0) {
		add_user_command (&ptr->type_commands,"show","Shows file data",type_file___show);
		add_user_command (&ptr->type_commands,"inode","Returns to the inode of the current file",type_file___inode);
		add_user_command (&ptr->type_commands,"display","Specifies data format - text or hex",type_file___display);
		add_user_command (&ptr->type_commands,"next","Pass to next byte",type_file___next);
		add_user_command (&ptr->type_commands,"prev","Pass to the previous byte",type_file___prev);
		add_user_command (&ptr->type_commands,"offset","Pass to a specified byte in the current block",type_file___offset);
		add_user_command (&ptr->type_commands,"nextblock","Pass to next file block",type_file___nextblock);
		add_user_command (&ptr->type_commands,"prevblock","Pass to the previous file block",type_file___prevblock);
		add_user_command (&ptr->type_commands,"block","Specify which file block to edit",type_file___block);
		add_user_command (&ptr->type_commands,"remember","Saves the file\'s inode position for later reference",type_file___remember);
		add_user_command (&ptr->type_commands,"set","Sets the current byte",type_file___set);
		add_user_command (&ptr->type_commands,"writedata","Writes the current block to the disk",type_file___writedata);
	}

	if (strcmp ((ptr->name),"ext2_inode")==0) {
		add_user_command (&ptr->type_commands,"show","Shows inode data",type_ext2_inode___show);
		add_user_command (&ptr->type_commands,"next","Move to next inode in current block group",type_ext2_inode___next);
		add_user_command (&ptr->type_commands,"prev","Move to next inode in current block group",type_ext2_inode___prev);
		add_user_command (&ptr->type_commands,"group","Move to the group descriptors of the current inode table",type_ext2_inode___group);
		add_user_command (&ptr->type_commands,"entry","Move to a specified entry in the current inode table",type_ext2_inode___entry);
		add_user_command (&ptr->type_commands,"file","Display file data of the current inode",type_ext2_inode___file);
		add_user_command (&ptr->type_commands,"dir","Display directory data of the current inode",type_ext2_inode___dir);
	}

	if (strcmp ((ptr->name),"dir")==0) {
		add_user_command (&ptr->type_commands,"show","Shows current directory data",type_dir___show);
		add_user_command (&ptr->type_commands,"inode","Returns to the inode of the current directory",type_dir___inode);
		add_user_command (&ptr->type_commands,"next","Pass to the next directory entry",type_dir___next);
		add_user_command (&ptr->type_commands,"prev","Pass to the previous directory entry",type_dir___prev);
		add_user_command (&ptr->type_commands,"followinode","Follows the inode specified in this directory entry",type_dir___followinode);
		add_user_command (&ptr->type_commands,"remember","Remember the inode of the current directory entry",type_dir___remember);
		add_user_command (&ptr->type_commands,"cd","Changes directory relative to the current directory",type_dir___cd);
		add_user_command (&ptr->type_commands,"entry","Moves to a specified entry in the current directory",type_dir___entry);
		add_user_command (&ptr->type_commands,"writedata","Writes the current entry to the disk",type_dir___writedata);
		add_user_command (&ptr->type_commands,"set","Changes a variable in the current directory entry",type_dir___set);
	}

	if (strcmp ((ptr->name),"ext2_super_block")==0) {
		add_user_command (&ptr->type_commands,"show","Displays the super block data",type_ext2_super_block___show);
		add_user_command (&ptr->type_commands,"gocopy","Move to another backup copy of the superblock",type_ext2_super_block___gocopy);
		add_user_command (&ptr->type_commands,"setactivecopy","Copies the current superblock to the main superblock",type_ext2_super_block___setactivecopy);
	}

	if (strcmp ((ptr->name),"ext2_group_desc")==0) {
		add_user_command (&ptr->type_commands,"next","Pass to the next block group decriptor",type_ext2_group_desc___next);
		add_user_command (&ptr->type_commands,"prev","Pass to the previous group descriptor",type_ext2_group_desc___prev);
		add_user_command (&ptr->type_commands,"entry","Pass to a specific group descriptor",type_ext2_group_desc___entry);
		add_user_command (&ptr->type_commands,"show","Shows the current group descriptor",type_ext2_group_desc___show);
		add_user_command (&ptr->type_commands,"inode","Pass to the inode table of the current group block",type_ext2_group_desc___inode);
		add_user_command (&ptr->type_commands,"gocopy","Move to another backup copy of the group descriptor",type_ext2_group_desc___gocopy);
		add_user_command (&ptr->type_commands,"blockbitmap","Show the block allocation bitmap of the current group block",type_ext2_group_desc___blockbitmap);
		add_user_command (&ptr->type_commands,"inodebitmap","Show the inode allocation bitmap of the current group block",type_ext2_group_desc___inodebitmap);
		add_user_command (&ptr->type_commands,"setactivecopy","Copies the current group descriptor to the main table",type_ext2_super_block___setactivecopy);
	}

	if (strcmp ((ptr->name),"block_bitmap")==0) {
		add_user_command (&ptr->type_commands,"show","Displays the block allocation bitmap",type_ext2_block_bitmap___show);
		add_user_command (&ptr->type_commands,"entry","Moves to a specific bit",type_ext2_block_bitmap___entry);
		add_user_command (&ptr->type_commands,"next","Moves to the next bit",type_ext2_block_bitmap___next);
		add_user_command (&ptr->type_commands,"prev","Moves to the previous bit",type_ext2_block_bitmap___prev);
		add_user_command (&ptr->type_commands,"allocate","Allocates the current block",type_ext2_block_bitmap___allocate);
		add_user_command (&ptr->type_commands,"deallocate","Deallocates the current block",type_ext2_block_bitmap___deallocate);
	}

	if (strcmp ((ptr->name),"inode_bitmap")==0) {
		add_user_command (&ptr->type_commands,"show","Displays the inode allocation bitmap",type_ext2_inode_bitmap___show);
		add_user_command (&ptr->type_commands,"entry","Moves to a specific bit",type_ext2_inode_bitmap___entry);
		add_user_command (&ptr->type_commands,"next","Moves to the next bit",type_ext2_inode_bitmap___next);
		add_user_command (&ptr->type_commands,"prev","Moves to the previous bit",type_ext2_inode_bitmap___prev);
		add_user_command (&ptr->type_commands,"allocate","Allocates the current inode",type_ext2_inode_bitmap___allocate);
		add_user_command (&ptr->type_commands,"deallocate","Deallocates the current inode",type_ext2_inode_bitmap___deallocate);
	}

}

void add_user_command (struct struct_commands *ptr,char *name,char *description,PF callback)

{
	int num;

	num=ptr->last_command;
	if (num+1==MAX_COMMANDS_NUM) {
		printf ("Internal Error - Can't add command %s\n",name);
		return;
	}

	ptr->last_command=++num;

	ptr->names [num]=(char *) malloc (strlen (name)+1);
	strcpy (ptr->names [num],name);

	if (*description!=0) {
		ptr->descriptions [num]=(char *) malloc (strlen (description)+1);
		strcpy (ptr->descriptions [num],description);
	}

	ptr->callback [num]=callback;
}

int set_file_system_info (void)

{
	int ext2_detected=0;
	struct ext2_super_block *sb;

	file_system_info.super_block_offset=1024;
	file_system_info.file_system_size=DefaultTotalBlocks*DefaultBlockSize;

	low_read ((char *) &file_system_info.super_block,sizeof (struct ext2_super_block),file_system_info.super_block_offset);

	sb=&file_system_info.super_block;

	if (sb->s_magic == EXT2_SUPER_MAGIC)
		ext2_detected=1;

	if (ext2_detected)
		wprintw (command_win,"Detected extended 2 file system on device %s\n",device_name);
	else
		wprintw (command_win,"Warning - Extended 2 filesystem not detected on device %s\n",device_name);

	if (!ext2_detected && !ForceExt2)
		wprintw (command_win,"You may wish to use the configuration option ForceExt2 on\n");

	if (ForceExt2 && !ext2_detected)
		wprintw (command_win,"Forcing extended 2 filesystem\n");

	if (ForceDefault || !ext2_detected)
		wprintw (command_win,"Forcing default parameters\n");

	refresh_command_win ();

	if (ext2_detected || ForceExt2) {
		add_ext2_general_commands ();
		if (!set_struct_descriptors (Ext2Descriptors))
			return (0);
	}

	if (!ForceDefault && ext2_detected) {

		file_system_info.block_size=EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size;
		if (file_system_info.block_size == EXT2_MIN_BLOCK_SIZE)
			file_system_info.first_group_desc_offset=2*EXT2_MIN_BLOCK_SIZE;
		else
			file_system_info.first_group_desc_offset=file_system_info.block_size;
		file_system_info.groups_count = ext2fs_div64_ceil(ext2fs_blocks_count(sb),
						 sb->s_blocks_per_group);

		file_system_info.inodes_per_block=file_system_info.block_size/sizeof (struct ext2_inode);
		file_system_info.blocks_per_group=sb->s_inodes_per_group/file_system_info.inodes_per_block;
		file_system_info.no_blocks_in_group=sb->s_blocks_per_group;
		file_system_info.file_system_size=(ext2fs_blocks_count(sb)-1)*file_system_info.block_size;
	}

	else {
		file_system_info.file_system_size=DefaultTotalBlocks*DefaultBlockSize;
		file_system_info.block_size=DefaultBlockSize;
		file_system_info.no_blocks_in_group=DefaultBlocksInGroup;
	}

	if (file_system_info.file_system_size > 2147483647) {
		wprintw (command_win,"Sorry, filesystems bigger than 2 GB are currently not supported\n");
		return (0);
	}
	return (1);
}

void init_readline (void)

{
#ifdef HAVE_READLINE
	rl_completion_entry_function=(Function *) complete_command;
#endif
}

void init_signals (void)

{
	signal (SIGWINCH, signal_SIGWINCH_handler);	/* Catch SIGWINCH */
	signal (SIGTERM, signal_SIGTERM_handler);
	signal (SIGSEGV, signal_SIGSEGV_handler);

}

void signal_SIGWINCH_handler (int sig_num)

{
	redraw_request=1;	/* We will handle it in main.c */

	/* Reset signal handler */
	signal (SIGWINCH, signal_SIGWINCH_handler);

}

void signal_SIGTERM_handler (int sig_num)

{
	prepare_to_close ();
	printf ("Terminated due to signal %d\n",sig_num);
	exit (1);
}

void signal_SIGSEGV_handler (int sig_num)

{
	prepare_to_close ();
	printf ("Killed by signal %d!\n",sig_num);
	exit (1);
}

int process_configuration_file (void)

{
	char buffer [300];
	char option [80],value [80];
	FILE *fp;

	strcpy (buffer, ROOT_SYSCONFDIR);
	strcat (buffer,"/ext2ed.conf");

	if ((fp=fopen (buffer,"rt"))==NULL) {
		fprintf (stderr,"Error - Unable to open configuration file %s\n",buffer);
		return (0);
	}

	while (get_next_option (fp,option,value)) {
		if (strcasecmp (option,"Ext2Descriptors")==0) {
			strcpy (Ext2Descriptors,value);
		}

		else if (strcasecmp (option,"AlternateDescriptors")==0) {
			strcpy (AlternateDescriptors,value);
		}

		else if (strcasecmp (option,"LogFile")==0) {
			strcpy (LogFile,value);
		}

		else if (strcasecmp (option,"LogChanges")==0) {
			if (strcasecmp (value,"on")==0)
				LogChanges = 1;
			else if (strcasecmp (value,"off")==0)
				LogChanges = 0;
			else {
				fprintf (stderr,"Error - Illegal value: %s %s\n",option,value);
				fclose (fp);return (0);
			}
		}

		else if (strcasecmp (option,"AllowChanges")==0) {
			if (strcasecmp (value,"on")==0)
				AllowChanges = 1;
			else if (strcasecmp (value,"off")==0)
				AllowChanges = 0;
			else {
				fprintf (stderr,"Error - Illegal value: %s %s\n",option,value);
				fclose (fp);return (0);
			}
		}

		else if (strcasecmp (option,"AllowMountedRead")==0) {
			if (strcasecmp (value,"on")==0)
				AllowMountedRead = 1;
			else if (strcasecmp (value,"off")==0)
				AllowMountedRead = 0;
			else {
				fprintf (stderr,"Error - Illegal value: %s %s\n",option,value);
				fclose (fp);return (0);
			}
		}

		else if (strcasecmp (option,"ForceExt2")==0) {
			if (strcasecmp (value,"on")==0)
				ForceExt2 = 1;
			else if (strcasecmp (value,"off")==0)
				ForceExt2 = 0;
			else {
				fprintf (stderr,"Error - Illegal value: %s %s\n",option,value);
				fclose (fp);return (0);
			}
		}

		else if (strcasecmp (option,"DefaultBlockSize")==0) {
			DefaultBlockSize = atoi (value);
		}

		else if (strcasecmp (option,"DefaultTotalBlocks")==0) {
			DefaultTotalBlocks = strtoul (value,NULL,10);
		}

		else if (strcasecmp (option,"DefaultBlocksInGroup")==0) {
			DefaultBlocksInGroup = strtoul (value,NULL,10);
		}

		else if (strcasecmp (option,"ForceDefault")==0) {
			if (strcasecmp (value,"on")==0)
				ForceDefault = 1;
			else if (strcasecmp (value,"off")==0)
				ForceDefault = 0;
			else {
				fprintf (stderr,"Error - Illegal value: %s %s\n",option,value);
				fclose (fp);return (0);
			}
		}

		else {
			fprintf (stderr,"Error - Unknown option: %s\n",option);
			fclose (fp);return (0);
		}
	}

	printf ("Configuration completed\n");
	fclose (fp);
	return (1);
}

int get_next_option (FILE *fp,char *option,char *value)

{
	char *ptr;
	char buffer [600];

	if (feof (fp)) return (0);
	do{
		if (feof (fp)) return (0);
		fgets (buffer,500,fp);
	} while (buffer [0]=='#' || buffer [0]=='\n');

	ptr=parse_word (buffer,option);
	ptr=parse_word (ptr,value);
	return (1);
}

void check_mounted (char *name)

{
	FILE *fp;
	char *ptr;
	char current_line [500],current_word [200];

	mounted=0;

	if ( (fp=fopen ("/etc/mtab","rt"))==NULL) {
		wprintw (command_win,"Error - Failed to open /etc/mtab. Assuming filesystem is mounted.\n");
		refresh_command_win ();mounted=1;return;
	};

	while (!feof (fp)) {
		fgets (current_line,500,fp);
		if (feof (fp)) break;
		ptr=parse_word (current_line,current_word);
		if (strcasecmp (current_word,name)==0) {
			mounted=1;fclose (fp);return;
		}
	};

	fclose (fp);

	return;
}

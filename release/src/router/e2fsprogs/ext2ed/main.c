/*

/usr/src/ext2ed/main.c

A part of the extended file system 2 disk editor.

------------
Main program
------------

This file mostly contains:

1.	A list of global variables used through the entire program.
2.	The parser, which asks the command line from the user.
3.	The dispatcher, which analyzes the command line and calls the appropriate handler function.
4.	A command pattern matcher which is used along with the readline completion feature.
5.	A function which tells the user that an internal error has occured.

First written on: March 30 1995

Copyright (C) 1995 Gadi Oxman

*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef HAVE_READLINE
#include <readline.h>
#include <history.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "ext2ed.h"

/* Global variables */

/*

Configuration file options

The following variables will be set by init.c to the values selected in the user configuration file.
They are initialized below to some logical defaults.

*/


char Ext2Descriptors [200]="ext2.descriptors";	/* The location of the ext2 filesystem object definition */
char AlternateDescriptors [200]="";		/* We allow the user to define additional structures */
char LogFile [200]="ext2ed.log";		/* The location of the log file - Each write will be logged there */
int LogChanges=1;				/* 1 enables logging, 0 diables logging */
int AllowChanges=0;				/* When set, the enablewrite command will fail */
int AllowMountedRead=0;				/* Behavior when trying to open a mounted filesystem read-only */
int ForceExt2=0;				/* When set, ext2 autodetection is overridden */
int DefaultBlockSize=1024;
unsigned long DefaultTotalBlocks=2097151;
unsigned long DefaultBlocksInGroup=8192;	/* The default values are used when an ext2 filesystem is not */
int ForceDefault=0;				/* detected, or ForceDefault is set */

char last_command_line [80];			/* A simple one command cache, in addition to the readline history */

char device_name [80];				/* The location of the filesystem */
FILE *device_handle=NULL;			/* This is passed to the fopen / fread ... commands */
long device_offset;				/* The current position in the filesystem */
						/* Note that we have a 2 GB limitation */

int mounted=0;					/* This is set when we find that the filesystem is mounted */

struct struct_commands general_commands,ext2_commands;		/* Used to define the general and ext2 commands */
struct struct_descriptor *first_type,*last_type,*current_type;	/* Used to access the double linked list */
struct struct_type_data type_data;				/* The current data is sometimes stored here */
struct struct_file_system_info file_system_info;		/* Essential information on the filesystem */
struct struct_file_info file_info,first_file_info;		/* Used by file_com.c to access files */
struct struct_group_info group_info;				/* Used by group_com.c */
struct struct_super_info super_info;				/* Used by super_com.c */
struct struct_remember_lifo remember_lifo;			/* A circular memory of objects */
struct struct_block_bitmap_info block_bitmap_info;		/* Used by blockbitmap_com.c */
struct struct_inode_bitmap_info inode_bitmap_info;		/* Used by inodebitmap_com.c */

int redraw_request=0;						/* Is set by a signal handler to handle terminal */
								/* screen size change. */


/*
 * We just call the parser to get commands from the user. We quit when
 * parser returns.
 */
int main (int argc, char **argv)
{
	int	write_priv = 0;
	int	c;
	char	*buf;

	if (!init ())
		return (1);
	while ((c = getopt (argc, argv, "w")) != EOF) {
		switch (c) {
		case 'w':
			write_priv++;
			break;
		}
	}
	if (optind < argc) {
		buf = malloc(strlen(argv[optind]) + 32);
		if (!buf) {
			fprintf(stderr, "Couldn't allocate filename buffer\n");
			exit(1);
		}
		strcpy(buf, "set_device ");
		strcat(buf, argv[optind]);
		set_device(buf);
		free(buf);
		if (write_priv) {
			wprintw (command_win,"\n");
			enable_write("enable_write");
		}
	}
	parser ();			/* Get and parse user commands */
	prepare_to_close();		/* Do some cleanup */
	printf("Quitting ...\n");
	return(0);
}


/*
 * Read a character from the command window
 */
int command_read_key()
{
	int	key = 0;

	while (!key) {
		if (redraw_request) {
			redraw_all();
			redraw_request=0;
		}
		key = wgetch(command_win);
		switch (key) {
		case 0x1A:
			key = 0;
			kill(getpid(), SIGTSTP);
			break;

		case KEY_NPAGE:
			pgdn("");
			refresh_command_win ();
			break;

		case KEY_PPAGE:
			pgup("");
			refresh_command_win ();
			break;
		case ERR:
			key = 0;
			break;

		case KEY_BACKSPACE:
			key = '\b';
		}
		if ((key < 32 && key != '\b' && key != '\n') ||
		    (key > 127))
			key = 0;
	}
	return key;
}

#ifdef HAVE_READLINE
int rl_getc_replacement(FILE *f)
{
	int	key = command_read_key();

	if (key == '\b') {
		if (rl_point > 0)
			wprintw(command_win, "\b \b");
	} else
		wprintw(command_win, "%c", key);
	return key;
}

/*
 * This function asks the user for a command and calls the dispatcher
 * function, dispatch, to analyze it.  We use the readline library
 * function readline to read the command, hence all the usual readline
 * keys are available.  The new command is saved both in the
 * readline's history and in our tiny one-command cache, so that only
 * the enter key is needed to retype it.
 */
void parser (void)
{
	char *ptr,command_line [80];
	int quit=0;

#if 0
	noecho();
	cbreak();
	keypad(command_win, 1);
	wtimeout(command_win, 100);

	rl_getc_function = rl_getc_replacement;
#endif

	while (!quit) {
		/* Terminal screen size has changed */
		if (redraw_request) {
			redraw_all();
			redraw_request=0;
		}

		wmove (command_win,0,0);
		wclrtoeol (command_win);
		wprintw (command_win,"ext2ed > ");
		refresh_command_win ();

		/*
		 * The ncurses library optimizes cursor movement by
		 * keeping track of the cursor position. However, by
		 * using the readline library I'm breaking its
		 * assumptions. The double -1 arguments tell ncurses
		 * to disable cursor movement optimization this
		 * time.
		 */
		mvcur (-1,-1,LINES-COMMAND_WIN_LINES,0);

		/* echo (); */
		ptr=readline ("ext2ed > ");
		/* noecho (); */

		/*
		 * Readline allocated the buffer - Copy the string
		 * and free the allocated buffer
		 * XXX WHY???
		 */
		strcpy (command_line,ptr);
		free (ptr);

		if (*command_line != 0)
			add_history (command_line);

		/* If only enter was pressed, recall the last command */
		if (*command_line==0)
			strcpy (command_line,last_command_line);

		/* Emulate readline's actions for ncurses */
		mvcur (-1,-1,LINES-COMMAND_WIN_LINES,0);
		werase (command_win);
		wprintw (command_win,"ext2ed > ");
		wprintw (command_win,command_line);
		wprintw (command_win,"\n");
		refresh_command_win ();

		/* Save this command in our tiny cache */
		strcpy (last_command_line,command_line);

		/* And call dispatch to do the actual job */
		quit=dispatch (command_line);
	}
}
#else
void read_line(char * foo) {
	char * chptr = foo;
	int ch;
	int done = 0;

	while (!done && (ch = command_read_key())) {
		switch (ch) {
		case '\n':
			done = 1;
			break;

		case '\b':
			if (chptr > foo) {
				wprintw(command_win, "\b \b");
				chptr--;
			}
			break;

		default:
			if (ch > 256)
				break;
			if (ch == '\n') break;
			*chptr++ = ch;
			wprintw(command_win, "%c", ch);
			break;
		}
	}
	*chptr = '\0';
}

void parser (void)
{
	char command_line [80];
	int quit=0;

	noecho();
	cbreak();
	wtimeout(command_win, 100);
	keypad(command_win, 1);

	while (!quit) {
		/* Terminal screen size has changed */
		if (redraw_request) {
			redraw_all();
			redraw_request=0;
		}

		wmove (command_win,0,0);wclrtoeol (command_win);

		wmove(command_win, 0, 0);
		wprintw(command_win, "ext2ed > ");
		read_line(command_line);

		/* If only enter was pressed, recall the last command */
 		if (*command_line==0)
 			strcpy (command_line,last_command_line);

		mvcur (-1,-1,LINES-COMMAND_WIN_LINES + 1,0);

 		strcpy (last_command_line,command_line);	/* Save this command in our tiny cache */

		/* And call dispatch to do the actual job */
		quit=dispatch (command_line);
	}
}
#endif


/*
 * This is a very important function. Its task is to recieve a command
 * name and link it to a C function.  There are three types of commands:
 *
 * 1.	General commands - Always available and accessed through
 * general_commands.
 * 2.	Ext2 specific commands - Available when editing an ext2
 * filesystem, accessed through ext2_commands.
 * 3.	Type specific commands - Those are changing according to the
 * current type. The global variable current_type points to the
 * current object definition (of type struct_descriptor). In it, the
 * struct_commands entry contains the type specific commands links.
 *
 * Overriding is an important feature - Much like in C++ : The same
 * command name can dispatch to different functions. The overriding
 * priority is 3,2,1; That is - A type specific command will always
 * override a general command. This is used through the program to
 * allow fine tuned operation.
 *
 * When an handling function is found, it is called along with the
 * command line that was passed to us. The handling function is then
 * free to interpert the arguments in its own style.
 */
int dispatch (char *command_line)
{
	int i,found=0;

	char command [80];

	parse_word (command_line,command);

	if (strcasecmp (command,"quit")==0) return (1);

	/* 1. Search for type specific commands FIRST - Allows
	overriding of a general command */

	if (current_type != NULL)
		for (i=0;
		     i<=current_type->type_commands.last_command && !found;
		     i++) {
			if (strcasecmp (command,current_type->type_commands.names [i])==0) {
				(*current_type->type_commands.callback [i]) (command_line);
				found=1;
			}
		}

	/* 2. Now search for ext2 filesystem general commands */

	if (!found)
		for (i=0;i<=ext2_commands.last_command && !found;i++) {
			if (strcasecmp (command,ext2_commands.names [i])==0) {
				(*ext2_commands.callback [i]) (command_line);
				found=1;
			}
		}


	/* 3. If not found, search the general commands */

	if (!found)
		for (i=0;i<=general_commands.last_command && !found;i++) {
			if (strcasecmp (command,general_commands.names [i])==0) {
				(*general_commands.callback [i]) (command_line);
				found=1;
			}
		}

	/* 4. If not found, issue an error message and return */

	if (!found) {
		wprintw (command_win,"Error: Unknown command\n");
		refresh_command_win ();
	}

	return (0);
}


/*
 *
 * This function copies the next word in source to the variable dest,
 * ignoring whitespaces.  It returns a pointer to the next word in
 * source.  It is used to split the command line into command and arguments.
 */
char *parse_word (char *source,char *dest)
{
	char ch,*source_ptr,*target_ptr;

	if (*source==0) {
		*dest=0;
		return (source);
	};

	source_ptr=source;target_ptr=dest;
	do {
		ch=*source_ptr++;
	} while (! (ch>' ' && ch<='z') && ch!=0);

	while (ch>' ' && ch<='z') {
		*target_ptr++=ch;
		ch=*source_ptr++;
	}

	*target_ptr=0;

	source_ptr--;
	do {
		ch=*source_ptr++;
	} while (! (ch>' ' && ch<='z') && ch!=0);

	return (--source_ptr);
}

/*
 * text is the partial command entered by the user; We assume that it
 * is a part of a command - I didn't write code for smarter completion.
 *
 * The state variable is an index which tells us how many possible
 * completions we already returned to readline.
 *
 * We return only one possible completion or (char *) NULL if there
 * are no more completions. This function will be called by readline
 * over and over until we tell it to stop.
 *
 * While scanning for possible completions, we use the same priority
 * definition which was used in dispatch.
 */
#if HAVE_READLINE
char *complete_command (char *text,int state)
{
	int state_index=-1;
	int i,len;

	len=strlen (text);

	/* Is the command type specific ? */

	if (current_type != NULL)
		for (i=0;i<=current_type->type_commands.last_command;i++) {
			if (strncmp (current_type->type_commands.names [i],text,len)==0) {
				state_index++;
				if (state==state_index) {
					return (dupstr (current_type->type_commands.names [i]));
				}
			}
		}

	/* No, pehaps ext2 specific command then ? */

	for (i=0;i<=ext2_commands.last_command;i++) {
		if (strncmp (ext2_commands.names [i],text,len)==0) {
			state_index++;
			if (state==state_index)
			return (dupstr (ext2_commands.names [i]));
		}
	}


	/* Check for a general command */

	for (i=0;i<=general_commands.last_command;i++) {
		if (strncmp (general_commands.names [i],text,len)==0) {
				state_index++;
				if (state==state_index)
					return (dupstr (general_commands.names [i]));
		}
	}

	/* quit is handled differently */

	if (strncmp ("quit",text,len)==0) {
		state_index++;
		if (state==state_index)
			return (dupstr ("quit"));
	}

	/* No more completions */

	return ((char *) NULL);
}
#endif


/*
 * Nothing special - Just allocates enough space and copy the string.
 */
char *dupstr (char *src)
{
	char *ptr;

	ptr=(char *) malloc (strlen (src)+1);
	strcpy (ptr,src);
	return (ptr);
}

#ifdef DEBUG
/*
 * This function reports an internal error. It is almost not used. One
 * place in which I do check for internal errors is disk.c.
 *
 * We just report the error, and try to continue ...
 */
void internal_error (char *description,char *source_name,char *function_name)
{
	wprintw (command_win,"Internal error - Found by source: %s.c , function: %s\n",source_name,function_name);
	wprintw (command_win,"\t%s\n",description);
	wprintw (command_win,"Press enter to (hopefully) continue\n");
	refresh_command_win ();getch ();werase (command_win);
}

#endif

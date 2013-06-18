
/*

/usr/src/ext2ed/ext2ed.h

A part of the extended file system 2 disk editor.

--------------------------------------
Include file for the ext2 disk editor.
--------------------------------------

This file contains declarations which are needed by all the files in ext2ed.

First written on: April 9 1995

Copyright (C) 1995 Gadi Oxman

*/

#ifndef EXT2ED_EDITOR_H
#define EXT2ED_EDITOR_H

/*

-----------------------
 User definable options
-----------------------

*/

#define DEBUG						/* Activate self-sanity checks */

#include <ext2fs/ext2_fs.h>				/* Main kernel ext2 include file */
#include <sys/stat.h>

#include <ncurses.h>

#define MAX_FIELDS 		400

#define MAX_COMMAND_LINE 	81
#define MAX_COMMANDS_NUM	30			/* Maximum number of commands of one type */
#define REMEMBER_COUNT		30			/* Object memory size */

/*
	The user screen consists of four parts:

		1.	Title window (title_win).
		2.	Show (status) window (show_win).
		3.	Main show pad (show_pad).
		4.	Command window (command_win).

*/

/*

   The show pad is mapped to the space left between the other three windows.

   If you wondered why ext2ed grabs so memory, the answer is probably below - I wanted to treat
   the virtual display as infinite. Decrease the following for more realistic memory consumption.

*/

#define SHOW_PAD_LINES 3000
#define SHOW_PAD_COLS (COLS > 140 ? COLS : 140)

#define COMMAND_WIN_LINES 6				/* Change this to your preferences */
#define TITLE_WIN_LINES 3
#define SHOW_WIN_LINES 3

#define HEX 1
#define TEXT 2

#ifndef EXT2_PRE_02B_MAGIC
	#define EXT2_PRE_02B_MAGIC	0xEF51
#endif


typedef void (*PF) (char *);				/* Used to point to the dispatched functions */

struct struct_commands {				/* Holds commands of an object */
	int last_command;
	char *names [MAX_COMMANDS_NUM];
	char *descriptions [MAX_COMMANDS_NUM];
	PF callback [MAX_COMMANDS_NUM];
};

struct struct_descriptor {				/* Describes an object */
	unsigned long length;
	unsigned char name [60];
	unsigned short fields_num;
	unsigned char field_names [MAX_FIELDS][80];
	unsigned char field_types [MAX_FIELDS];
	unsigned short field_lengths [MAX_FIELDS];
	unsigned short field_positions [MAX_FIELDS];
	struct struct_commands type_commands;
	struct struct_descriptor *prev,*next;
};

#define FIELD_TYPE_INT	  1
#define FIELD_TYPE_UINT   2
#define FIELD_TYPE_CHAR   3

struct struct_type_data {				/* The object's data is usually here */
	long offset_in_block;

	union union_type_data {				/* Format it in various ways */
		char buffer [EXT2_MAX_BLOCK_SIZE];
		struct ext2_acl_header t_ext2_acl_header;
		struct ext2_acl_entry t_ext2_acl_entry;
		struct ext2_group_desc t_ext2_group_desc;
		struct ext2_inode t_ext2_inode;
		struct ext2_super_block t_ext2_super_block;
		struct ext2_dir_entry t_ext2_dir_entry;
	} u;
};

struct struct_file_system_info {			/* Important information about the filesystem */
	unsigned long long file_system_size;
	unsigned long super_block_offset;
	unsigned long first_group_desc_offset;
	unsigned long groups_count;
	unsigned long inodes_per_block;
	unsigned long blocks_per_group;			/* The name is misleading; beware */
	unsigned long no_blocks_in_group;
	unsigned short block_size;
	struct ext2_super_block super_block;
};

struct struct_file_info {				/* Used to handle files and directories */

	struct ext2_inode *inode_ptr;

	long inode_offset;
	long global_block_num,global_block_offset;
	long block_num,blocks_count;
	long file_offset,file_length;
	long level;
	unsigned char buffer [EXT2_MAX_BLOCK_SIZE];
	long offset_in_block;

	int display;
	/* The following is used if the file is a directory */

	long dir_entry_num,dir_entries_count;
	long dir_entry_offset;
};

struct struct_super_info {				/* Used to handle the superblock */
	unsigned long copy_num;
};

struct struct_group_info {				/* Used to handle the group descriptors */
	unsigned long copy_num;
	unsigned long group_num;
};

struct struct_block_bitmap_info {			/* Used in blockbitmap_com.c */
	unsigned long entry_num;
	unsigned long group_num;
};

struct struct_inode_bitmap_info {			/* Used in inodebitmap_com.c */
	unsigned long entry_num;
	unsigned long group_num;
};

struct struct_remember_lifo {				/* Implements the objects circular memory */
	long entries_count;

	long offset [REMEMBER_COUNT];
	struct struct_descriptor *type [REMEMBER_COUNT];
	char name [REMEMBER_COUNT][80];
};

struct struct_pad_info {				/* Used to zoom into the pad window */
	int display_lines,display_cols;
	int line,col;
	int max_line,max_col;
	int disable_output;
};

/* Global variables (defined mostly in main.c) */

/* Configurable variables (Through configuration file) */

extern char AlternateDescriptors [200];
extern char Ext2Descriptors [200];
extern char LogFile [200];
extern int LogChanges;
extern int AllowChanges;
extern int AllowMountedRead;
extern int ForceExt2;
extern int DefaultBlockSize;
extern unsigned long DefaultTotalBlocks;
extern unsigned long DefaultBlocksInGroup;
extern int ForceDefault;

extern char device_name [80];
extern char last_command_line [80];
extern FILE *device_handle;
extern long device_offset;
extern int  mounted;

extern short block_size;
extern struct struct_commands general_commands;
extern struct struct_commands ext2_commands;
extern struct struct_descriptor *first_type;
extern struct struct_descriptor *last_type;
extern struct struct_descriptor *current_type;
extern struct struct_type_data type_data;
extern struct struct_file_system_info file_system_info;
extern struct struct_file_info file_info,first_file_info;
extern struct struct_group_info group_info;
extern struct struct_super_info super_info;
extern struct struct_block_bitmap_info block_bitmap_info;
extern struct struct_inode_bitmap_info inode_bitmap_info;
extern struct struct_remember_lifo remember_lifo;
extern struct struct_pad_info show_pad_info;
extern int write_access;

extern int redraw_request;
extern char lines_s [80];
extern char cols_s [80];


/* init.c */

extern int init (void);
extern void prepare_to_close (void);
extern int set_struct_descriptors (char *file_name);
extern void free_struct_descriptors (void);
extern struct struct_descriptor *add_new_descriptor (char *name);
extern void add_new_variable (struct struct_descriptor *descriptor,char *v_type,char *v_name);
extern void fill_type_commands (struct struct_descriptor *ptr);
extern void add_user_command (struct struct_commands *ptr,char *name,char *description,PF callback);
extern void free_user_commands (struct struct_commands *ptr);
extern int set_file_system_info (void);
extern int process_configuration_file (void);
extern void add_general_commands (void);
extern void add_ext2_general_commands (void);
extern void check_mounted (char *name);

int get_next_option (FILE *fp,char *option,char *value);
void init_readline (void);
void init_signals (void);
void signal_SIGWINCH_handler (int sig_num);
void signal_SIGTERM_handler (int sig_num);
void signal_SIGSEGV_handler (int sig_num);

/* general_com.c */

/* General commands which are aviable always */

extern void help (char *command_line);
extern void set (char *command_line);
extern void set_device (char *command_line);
extern void set_offset (char *command_line);
extern void set_type (char *command_line);
extern void show (char *command_line);
extern void pgup (char *command_line);
extern void pgdn (char *command_line);
extern void redraw (char *command_line);
extern void remember (char *command_line);
extern void recall (char *command_line);
extern void cd (char *command_line);
extern void enable_write (char *command_line);
extern void disable_write (char *command_line);
extern void write_data (char *command_line);
extern void next (char *command_line);
extern void prev (char *command_line);

void hex_set (char *command_line);
void detailed_help (char *text);


/* ext2_com.c */

/* Extended2 filesystem genereal commands - Aviable only when editing an
   ext2 filesystem */

extern void type_ext2___super (char *command_line);
extern void type_ext2___group (char *command_line);
extern void type_ext2___cd (char *command_line);


/* main.c */

extern int version_major,version_minor;
extern char revision_date [80];
extern char email_address [80];

#ifdef DEBUG
extern void internal_error (char *description,char *source_name,char *function_name);
#endif

void parser (void);
extern int dispatch (char *command_line);
char *parse_word (char *source,char *dest);
char *complete_command (char *text,int state);
char *dupstr (char *src);



/* disk.c */

extern int load_type_data (void);
extern int write_type_data (void);
extern int low_read (unsigned char *buffer,unsigned long length,unsigned long offset);
extern int low_write (unsigned char *buffer,unsigned long length,unsigned long offset);
extern int log_changes (unsigned char *buffer,unsigned long length,unsigned long offset);

/* file_com.c */

extern int init_file_info (void);
extern void type_file___show (char *command_line);
extern void type_file___inode (char *command_line);
extern void type_file___display (char *command_line);
extern void type_file___prev (char *command_line);
extern void type_file___next (char *command_line);
extern void type_file___offset (char *command_line);
extern void type_file___prevblock (char *command_line);
extern void type_file___nextblock (char *command_line);
extern void type_file___block (char *command_line);
extern void type_file___remember (char *command_line);
extern void type_file___set (char *command_line);
extern void type_file___writedata (char *command_line);

extern long file_block_to_global_block (long file_block,struct struct_file_info *file_info_ptr);
extern long return_indirect (long table_block,long block_num);
extern long return_dindirect (long table_block,long block_num);
extern long return_tindirect (long table_block,long block_num);

void file_show_hex (void);
void file_show_text (void);
void show_status (void);

/* inode_com.c */

extern void type_ext2_inode___next (char *command_line);
extern void type_ext2_inode___prev (char *command_line);
extern void type_ext2_inode___show (char *command_line);
extern void type_ext2_inode___group (char *command_line);
extern void type_ext2_inode___entry (char *command_line);
extern void type_ext2_inode___file (char *command_line);
extern void type_ext2_inode___dir (char *command_line);

extern long inode_offset_to_group_num (long inode_offset);
extern long int inode_offset_to_inode_num (long inode_offset);
extern long int inode_num_to_inode_offset (long inode_num);

/* dir_com.c */

extern int init_dir_info (struct struct_file_info *info);
extern void type_dir___show (char *command_line);
extern void type_dir___inode (char *command_line);
extern void type_dir___pgdn (char *command_line);
extern void type_dir___pgup (char *command_line);
extern void type_dir___prev (char *command_line);
extern void type_dir___next (char *command_line);
extern void type_dir___followinode (char *command_line);
extern void type_dir___remember (char *command_line);
extern void type_dir___cd (char *command_line);
extern void type_dir___entry (char *command_line);
extern void type_dir___writedata (char *command_line);
extern void type_dir___set (char *command_line);

#define HEX 1
#define TEXT 2

#define ABORT		0
#define CONTINUE	1
#define FOUND		2

struct struct_file_info search_dir_entries (int (*action) (struct struct_file_info *info),int *status);
int action_count (struct struct_file_info *info);
void show_dir_status (void);
long count_dir_entries (void);
int action_name (struct struct_file_info *info);
int action_entry_num (struct struct_file_info *info);
int action_show (struct struct_file_info *info);

/* super_com.c */

extern void type_ext2_super_block___show (char *command_line);
extern void type_ext2_super_block___gocopy (char *command_line);
extern void type_ext2_super_block___setactivecopy (char *command_line);

/* group_com.c */

extern void type_ext2_group_desc___next (char *command_line);
extern void type_ext2_group_desc___prev (char *command_line);
extern void type_ext2_group_desc___entry (char *command_line);
extern void type_ext2_group_desc___show (char *command_line);
extern void type_ext2_group_desc___inode (char *command_line);
extern void type_ext2_group_desc___gocopy (char *command_line);
extern void type_ext2_group_desc___blockbitmap (char *command_line);
extern void type_ext2_group_desc___inodebitmap (char *command_line);
extern void type_ext2_group_desc___setactivecopy (char *command_line);

/* blockbitmap_com.c */

extern void type_ext2_block_bitmap___show (char *command_line);
extern void type_ext2_block_bitmap___entry (char *command_line);
extern void type_ext2_block_bitmap___next (char *command_line);
extern void type_ext2_block_bitmap___prev (char *command_line);
extern void type_ext2_block_bitmap___allocate (char *command_line);
extern void type_ext2_block_bitmap___deallocate (char *command_line);
void allocate_block (long entry_num);
void deallocate_block (long entry_num);

/* inodebitmap_bom.c */

extern void type_ext2_inode_bitmap___show (char *command_line);
extern void type_ext2_inode_bitmap___entry (char *command_line);
extern void type_ext2_inode_bitmap___next (char *command_line);
extern void type_ext2_inode_bitmap___prev (char *command_line);
extern void type_ext2_inode_bitmap___allocate (char *command_line);
extern void type_ext2_inode_bitmap___deallocate (char *command_line);
void allocate_inode (long entry_num);
void deallocate_inode (long entry_num);

/* win.c */

extern WINDOW *title_win,*show_win,*command_win,*show_pad;

extern void init_windows (void);
extern void refresh_title_win (void);
extern void refresh_show_win (void);
extern void refresh_show_pad (void);
extern void refresh_command_win (void);
extern void show_info (void);
extern void redraw_all (void);
extern void close_windows (void);

#endif /* EXT2ED_EDITOR_H */

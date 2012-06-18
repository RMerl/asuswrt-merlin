/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: linelist.h,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $
 ***************************************************************************/



#ifndef _LINELIST_H_
#define _LINELIST_H_ 1

/*
 * arrays of pointers to lines
 */


#define cval(x) (int)(*(unsigned const char *)(x))

struct line_list {
	char **list;	/* array of pointers to lines */
	int count;		/* number of entries */
	int max;		/* maximum number of entries */
};

typedef void (WorkerProc)();

/*
 * data structure for job
 */

struct job{
	char sort_key[512]; /* sort key for job */

	/* information about job in key=value format */
	struct line_list info;

	/* data file information
	   This actually an array of line_list structures
		that has the data file information indexed by
		keyword.  There is one of each of these entries
		for each 'block' of datafiles in the original control
		file.
	 */
	struct line_list datafiles;

	/* routing information for a job
		This is really a list of lists for each destination
		The information per destination is accessed by keyword
	 */
	 
	struct line_list destination;
};

/*
 * Types of options that we can initialize or set values of
 */
#define FLAG_K		0
#define INTEGER_K	1
#define	STRING_K	2

/*
 * datastructure for initialization
 */

struct keywords{
    char *keyword;		/* name of keyword */
	char *translation;	/* translation for display */
    int type;			/* type of entry */
    void *variable;		/* address of variable */
	int  maxval;		/* value of token */
	int  flag;			/* flag for variable */
	char *default_value;		/* default value */
};

struct jobwords{
    const char **keyword;		/* name of value in job->info */
    int type;			/* type of entry */
    void *variable;		/* address of variable */
	int  maxlen;		/* length of value */
	char *key;			/* key we use for value */
};

/*
 * Variables
 */
extern struct keywords Pc_var_list[], DYN_var_list[];
/* we need to free these when we initialize */

EXTERN struct line_list
	Config_line_list, PC_filters_line_list,
	PC_names_line_list, PC_order_line_list,
	PC_info_line_list, PC_entry_line_list, PC_alias_line_list,
	/*
	User_PC_names_line_list, User_PC_order_line_list,
	User_PC_info_line_list, User_PC_alias_line_list,
	*/
	All_line_list, Spool_control, Sort_order,
	RawPerm_line_list, Perm_line_list, Perm_filters_line_list,
	Process_list, Tempfiles, Servers_line_list, Printer_list,
	Files, Status_lines, Logger_line_list, RemoteHost_line_list;
EXTERN struct line_list *Allocs[]
#ifdef DEFS
	 ={
	 &Config_line_list, &PC_filters_line_list,
	 &PC_names_line_list, &PC_order_line_list,
	 &PC_info_line_list, &PC_entry_line_list, &PC_alias_line_list,
	/*
	 &User_PC_names_line_list, &User_PC_order_line_list,
	 &User_PC_info_line_list, &User_PC_alias_line_list,
	*/
	 &All_line_list, &Spool_control, &Sort_order,
	 &RawPerm_line_list, &Perm_line_list, &Perm_filters_line_list,
	 &Tempfiles, &Servers_line_list,
	 &Printer_list, &Files, &Status_lines, &Logger_line_list, &RemoteHost_line_list,
	0 }
#endif
	;


/*
 * Constants
 */
EXTERN char *Value_sep DEFINE( = " \t=#@" );
EXTERN char *Whitespace DEFINE( = " \t\n\f" );
EXTERN char *List_sep DEFINE( = "[] \t\n\f," );
EXTERN char *Linespace DEFINE( = " \t" );
EXTERN char *File_sep DEFINE( = " \t,;:" );
EXTERN char *Strict_file_sep DEFINE( = ";:" );
EXTERN char *Perm_sep DEFINE( = "=,;" );
EXTERN char *Arg_sep DEFINE( = ",;" );
EXTERN char *Name_sep DEFINE( = "|:" );
EXTERN char *Line_ends DEFINE( = "\n\014\004\024" );
EXTERN char *Line_ends_and_colon DEFINE( = "\n\014\004\024:" );
EXTERN char *Printcap_sep DEFINE( = "|:" );
EXTERN char *Host_sep DEFINE( = "{} \t," );

/* PROTOTYPES */
void lowercase( char *s );
void uppercase( char *s );
char *trunc_str( char *s);
int Lastchar( char *s );
void *malloc_or_die( size_t size, const char *file, int line );
void *realloc_or_die( void *p, size_t size, const char *file, int line );
char *safestrdup (const char *p, const char *file, int line);
char *safestrdup2( const char *s1, const char *s2, const char *file, int line );
char *safeextend2( char *s1, const char *s2, const char *file, int line );
char *safestrdup3( const char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend3( char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend4( char *s1, const char *s2, const char *s3, const char *s4,
	const char *file, int line );
char *safestrdup4( const char *s1, const char *s2,
	const char *s3, const char *s4,
	const char *file, int line );
char *safeextend5( char *s1, const char *s2, const char *s3, const char *s4, const char *s5,
	const char *file, int line );
char *safestrdup5( const char *s1, const char *s2,
	const char *s3, const char *s4, const char *s5,
	const char *file, int line );
void Init_line_list( struct line_list *l );
void Free_line_list( struct line_list *l );
void Free_listof_line_list( struct line_list *l );
void Check_max( struct line_list *l, int incr );
char *Add_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq );
void Add_casekey_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq );
void Prefix_line_list( struct line_list *l, char *str );
void Merge_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq );
void Merge_listof_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq );
void Move_line_list( struct line_list *dest, struct line_list *src );
void Split( struct line_list *l, char *str, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments, char *escape );
char *Join_line_list( struct line_list *l, char *sep );
char *Join_line_list_with_sep( struct line_list *l, char *sep );
char *Join_line_list_with_quotes( struct line_list *l, char *sep );

void Dump_line_list( const char *title, struct line_list *l );
void Dump_line_list_sub( const char *title, struct line_list *l );
char *Find_str_in_flat( char *str, const char *key, const char *sep );
int Find_last_key( struct line_list *l, const char *key, const char *sep, int *m );
int Find_last_casekey( struct line_list *l, const char *key, const char *sep, int *m );
int Find_first_key( struct line_list *l, const char *key, const char *sep, int *m );
int Find_first_casekey( struct line_list *l, const char *key, const char *sep, int *m );
const char *Find_value( struct line_list *l, const char *key, const char *sep );
char *Find_first_letter( struct line_list *l, const char letter, int *mid );
const char *Find_exists_value( struct line_list *l, const char *key, const char *sep );
char *Find_str_value( struct line_list *l, const char *key, const char *sep );
char *Find_casekey_str_value( struct line_list *l, const char *key, const char *sep );
void Set_str_value( struct line_list *l, const char *key, const char *value );
void Set_expanded_str_value( struct line_list *l, const char *key, const char *orig );
void Set_casekey_str_value( struct line_list *l, const char *key, const char *value );
void Set_flag_value( struct line_list *l, const char *key, long value );
void Set_nz_flag_value( struct line_list *l, const char *key, long value );
void Set_double_value( struct line_list *l, const char *key, double value );
void Set_decimal_value( struct line_list *l, const char *key, long value );
void Remove_line_list( struct line_list *l, int mid );
void Remove_duplicates_line_list( struct line_list *l );
int Find_flag_value( struct line_list *l, const char *key, const char *sep );
int Find_decimal_value( struct line_list *l, const char *key, const char *sep );
double Find_double_value( struct line_list *l, const char *key, const char *sep );
const char *Fix_val( const char *s );
void Find_tags( struct line_list *dest, struct line_list *l, char *key );
void Find_default_tags( struct line_list *dest,
	struct keywords *var_list, char *tag );
void Read_file_list( int required, struct line_list *model, char *str,
	const char *linesep, int sort, const char *keysep, int uniq, int trim,
	int marker, int doinclude, int nocomment, int depth, int maxdepth );
void Read_fd_and_split( struct line_list *list, int fd,
	const char *linesep, int sort, const char *keysep, int uniq,
	int trim, int nocomment );
void Read_file_and_split( struct line_list *list, char *file,
	const char *linesep, int sort, const char *keysep, int uniq,
	int trim, int nocomment );
int  Build_pc_names( struct line_list *names, struct line_list *order,
	char *str, struct host_information *hostname  );
void Build_printcap_info( 
	struct line_list *names, struct line_list *order,
	struct line_list *list, struct line_list *raw,
	struct host_information *hostname );
char *Select_pc_info( const char *id,
	struct line_list *info,
	struct line_list *aliases,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input,
	int depth, int wildcard );
void Find_pc_info( char *name,
	struct line_list *info,
	struct line_list *aliases,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input,
	int depth, int wildcard );
void Clear_var_list( struct keywords *v, int setv );
void Set_var_list( struct keywords *keys, struct line_list *values );
int Check_str_keyword( const char *name, int *value );
#if defined(JYWENG20031104Config_value_conversion)
void Config_value_conversion( struct keywords *key, const char *s );
#else
#define Config_value_conversion(...) NULL
#endif
void Expand_percent( char **var );
void Expand_vars( void );
void Expand_hash_values( struct line_list *hash );
char *Set_DYN( char **v, const char *s );
void Clear_config( void );
char *Find_default_var_value( void *v );
void Get_config( int required, char *path );
void Reset_config( void );
void close_on_exec( int fd );
void Setup_env_for_process( struct line_list *env, struct job *job );
void Getprintcap_pathlist( int required,
	struct line_list *raw, struct line_list *filters,
	char *path );
void Filterprintcap( struct line_list *raw, struct line_list *filters,
	const char *str );
int In_group( char *group, char *user );
int Check_for_rg_group( char *user );
char *Init_tempfile( void );
int Make_temp_fd_in_dir( char **temppath, char *dir );
int Make_temp_fd( char **temppath );
void Clear_tempfile_list(void);
void Unlink_tempfiles(void);
void Remove_tempfiles(void);
void Split_cmd_line( struct line_list *l, char *line );
int Make_passthrough( char *line, char *flags, struct line_list *passfd,
	struct job *job, struct line_list *env_init );
int Filter_file( int input_fd, int output_fd, char *error_header,
	char *pgm, char * filter_options, struct job *job,
	struct line_list *env, int verbose );
char *Is_clean_name( char *s );
void Clean_name( char *s );
int Is_meta( int c );
char *Find_meta( char *s );
void Clean_meta( char *t );
void Dump_parms( char *title, struct keywords *k );
void Dump_default_parms( int fd, char *title, struct keywords *k );
void Remove_sequential_separators( char *start );
void Fix_Z_opts( struct job *job );
void Fix_dollars( struct line_list *l, struct job *job, int nosplit, char *flags );
char *Make_pathname( const char *dir,  const char *file );
int Get_keyval( char *s, struct keywords *controlwords );
char *Get_keystr( int c, struct keywords *controlwords );
char *Escape( char *str, int level );
void Escape_colons( struct line_list *list );
void Unescape( char *str );
char *Find_str_in_str( char *str, const char *key, const char *sep );
int Find_key_in_list( struct line_list *l, const char *key, const char *sep, int *m );
char *Fix_str( char *str );
int Shutdown_or_close( int fd );
void Setup_lpd_call( struct line_list *passfd, struct line_list *args );
int Make_lpd_call( char *name, struct line_list *passfd, struct line_list *args );
void Do_work( char *name, struct line_list *args );
int Start_worker( char *name, struct line_list *parms, int fd );

#endif

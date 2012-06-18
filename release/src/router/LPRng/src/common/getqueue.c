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
 *
 ***************************************************************************/

 static char *const _id =
"$Id: getqueue.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


/***************************************************************************
 * Commentary
 * Patrick Powell Thu Apr 27 21:48:38 PDT 1995
 * 
 * The spool directory holds files and other information associated
 * with jobs.  Job files have names of the form cfXNNNhostname.
 * 
 * The Scan_queue routine will scan a spool directory, looking for new
 * or missing control files.  If one is found,  it will add it to
 * the control file list.  It will then sort the list of file names.
 * 
 * In order to prevent strange things with pointers,  you should not store
 * pointers to this list,  but use indexes instead.
 ***************************************************************************/

#include "lp.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linelist.h"
#include "getprinter.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "globmatch.h"
#include "permission.h"
#include "lockfile.h"
#include "merge.h"

#if defined(USER_INCLUDE)
# include USER_INCLUDE
#else
# if defined(ORDER_ROUTINE)
#   error No 'USER_INCLUDE' file with ORDER_ROUTINE function prototypes specified
    You need an include file with function prototypes
# endif
#endif

/**** ENDINCLUDE ****/
/*
 * We make the following assumption:
 *   a hold file is the thing that LPRng will create.  A control file without
 *   a hold file is an orphan,  and should be disposed of.  We will ignore these
 *   and leave it to the checkpc code to clean them up.
 */

#if defined(JY20031104Scan_queue)
int Scan_queue( struct line_list *spool_control,
	struct line_list *sort_order, int *pprintable, int *pheld, int *pmove,
		int only_queue_process, int *perr, int *pdone,
		const char *remove_prefix, const char *remove_suffix )
{
	DIR *dir;						/* directory */
	struct dirent *d;				/* directory entry */
	struct line_list directory_files;
	char *hf_name;
	int c, printable, held, move, error, done, p, h, m, e, dn;
	int remove_prefix_len = safestrlen( remove_prefix );
	int remove_suffix_len = safestrlen( remove_suffix );
	struct job job;

	c = printable = held = move = error = done = 0;
	Init_job( &job );
	Init_line_list(&directory_files);
	if( pprintable ) *pprintable = 0;
	if( pheld ) *pheld = 0;
	if( pmove ) *pmove = 0;
	if( perr ) *perr = 0;
	if( pdone ) *pdone = 0;

	Free_line_list(sort_order);

	if( !(dir = opendir( "." )) ){
		return(1);
	}

	hf_name = 0;
	while(1){
		while( (d = readdir(dir)) ){
			hf_name = d->d_name;
			DEBUG5("Scan_queue: found file '%s'", hf_name );
			if(
				(remove_prefix_len && !strncmp( hf_name, remove_prefix, remove_prefix_len ) )
				|| (remove_suffix_len 
					&& !strcmp( hf_name+strlen(hf_name)-remove_suffix_len, remove_suffix ))
			){
				DEBUG1("Scan_queue: removing file '%s'", hf_name );
				unlink( hf_name );
				continue;
			} else if(    (cval(hf_name+0) == 'h')
				&& (cval(hf_name+1) == 'f')
				&& isalpha(cval(hf_name+2))
				&& isdigit(cval(hf_name+3))
				){
				break;
			}
		}
		/* found them all */
		if( d == 0 ){
			break;
		}

		Free_job( &job );

		DEBUG2("Scan_queue: processing file '%s'", hf_name );

		/* read the hf file and get the information */
		Get_file_image_and_split( hf_name, 0, 0,
			&job.info, Line_ends, 1, Value_sep,1,1,1,0);
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL5)Dump_line_list("Scan_queue: hf", &job.info );
#endif
		if( job.info.count == 0 ){
			continue;
		}

		/* get the data file from the control file */
		Setup_cf_info( &job, 1 );
		Job_printable(&job,spool_control, &p,&h,&m,&e,&dn);
		if( p ) ++printable;
		if( h ) ++held;
		if( m ) ++move;
		if( e ) ++error;
		if( dn ) ++done;

		/* now generate the sort key */
		DEBUG4("Scan_queue: p %d, m %d, e %d, dn %d, only_queue_process %d",
			p, m, e, dn, only_queue_process );
		if( sort_order ){
			if( !only_queue_process || (p || m || e || dn) ){
#ifdef ORIGINAL_DEBUG//JY@1020
				if(DEBUGL4)Dump_job("Scan_queue - before Make_sort_key",&job);
#endif
				Make_sort_key( &job );
				DEBUG5("Scan_queue: sort key '%s'",job.sort_key);
				Set_str_value(sort_order,job.sort_key,hf_name);
			}
		}
	}
	closedir(dir);

	Free_job(&job);
	Free_line_list(&directory_files);

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL5){
		LOGDEBUG("Scan_queue: final values" );
		Dump_line_list_sub(SORT_KEY,sort_order);
	}
#endif
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	if( perr ) *perr = error;
	if( pdone ) *pdone = done;
	DEBUG3("Scan_queue: final printable %d, held %d, move %d, error %d, done %d",
		printable, held, move, error, done );
	return(0);
}
#endif

/*
 * char *Get_fd_image( int fd, char *file )
 *  Get an image of a file from an fd
 */

char *Get_fd_image( int fd, off_t maxsize )
{
	char *s = 0;
	struct stat statb;
	char buffer[LARGEBUFFER];
	int n;
	off_t len;

	DEBUG3("Get_fd_image: fd %d", fd );

	if( lseek(fd, 0, SEEK_SET) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Get_fd_image: lseek failed" );
	}
	if( maxsize && fstat(fd, &statb) == 0
		&& maxsize< statb.st_size/1024 ){
		lseek(fd, -maxsize*1024, SEEK_END);
	}
	
	len = 0;
	while( (n = read(fd,buffer,sizeof(buffer))) > 0 ){
		s = realloc_or_die(s,len+n+1,__FILE__,__LINE__);
		memcpy(s+len,buffer,n);
		len += n;
		s[len] = 0;
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3){
		SNPRINTF(buffer,32)"%s",s);
		logDebug("Get_fd_image: len %d '%s'", s?safestrlen(s):0, buffer );
	}
#endif
	return(s);
}

/*
 * char *Get_file_image( char *dir, char *file )
 *  Get an image of a file
 */

char *Get_file_image( const char *file, off_t maxsize )
{
	char *s = 0;
	struct stat statb;
	int fd;

	if( file == 0 ) return(0);
	DEBUG3("Get_file_image: '%s', maxsize %d", file, maxsize );
	if( (fd = Checkread( file, &statb )) >= 0 ){
		s = Get_fd_image( fd, maxsize );
		close(fd);
	}
	return(s);
}

/*
 * char *Get_fd_image_and_split
 *  Get an image of a file
 */

int Get_fd_image_and_split( int fd,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image )
{
	char *s = 0;
	if( return_image ) *return_image = 0;
	s = Get_fd_image( fd, maxsize );
	if( s == 0 ) return 1;
	if( clean ) Clean_meta(s);
	Split( l, s, sep, sort, keysep, uniq, trim, nocomments ,0);
	if( return_image ){
		*return_image = s;
	} else {
		if( s ) free(s); s = 0;
	}
	return(0);
}


/*
 * char *Get_file_image_and_split
 *  Get an image of a file
 */

int Get_file_image_and_split( const char *file,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image )
{
	char *s = 0;
	if( return_image ) *return_image = 0;
	if( !ISNULL(file) ) s = Get_file_image( file, maxsize );
	if( s == 0 ) return 1;
	if( clean ) Clean_meta(s);
	Split( l, s, sep, sort, keysep, uniq, trim, nocomments ,0);
	if( return_image ){
		*return_image = s;
	} else {
		if( s ) free(s); s = 0;
	}
	return(0);
}

/*
 * Set up a job data structure with information from the
 *   file images
 *  - Check_for_hold -
 *     checks to see if the job is held by class or by
 *     command
 *  Setup_job - gets the job information and updates it
 *     from the spool queue and control information
 */


void Check_for_hold( struct job *job, struct line_list *spool_control )
{
	int held, i;
	/* get the hold class of the job */
	held = Get_hold_class(&job->info,spool_control);
	Set_flag_value(&job->info,HOLD_CLASS,held);

	/* see if we need to hold this job by time */
	if( !Find_exists_value(&job->info,HOLD_TIME,Value_sep) ){
		if( Find_flag_value(spool_control,HOLD_TIME,Value_sep) ){
			i = time((void *)0);
		} else {
			i = 0;
		}
		Set_flag_value( &job->info, HOLD_TIME, i );
	}
	held = Find_flag_value(&job->info,HOLD_TIME,Value_sep);
	Set_flag_value(&job->info,HELD,held);
}

/*
 * Setup_job: called only when you REALLY want to
 *  read the control file and hold file.
 */

#if defined(JY20031104Setup_job)
void Setup_job( struct job *job, struct line_list *spool_control,
	const char *cf_name, const char *hf_name, int check_for_existence )
{
	struct stat statb;
	char *path, *s;
	struct line_list *lp;
	int i, j, size = 0;

	/* add the hold file information directly */ 
	DEBUG3("Setup_job: hf_name '%s', cf_name (TRANSFERNAME) '%s'", hf_name, cf_name );
	if( cf_name ){
		Set_str_value(&job->info,TRANSFERNAME, cf_name);
	}
	cf_name = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	if( hf_name ){
		Set_str_value(&job->info,HF_NAME,hf_name);
	}
	hf_name = Find_str_value(&job->info,HF_NAME,Value_sep);

	if( cf_name && !Find_str_value(&job->info,NUMBER,Value_sep) ){
		Check_format( CONTROL_FILE, cf_name, job );
	}

	if( !Find_str_value(&job->info,JOB_TIME,Value_sep)
		&& (path = Find_str_value(&job->info,OPENNAME,Value_sep)) ){
		j = 0;
		if( stat(path,&statb) ){
			i = time((void *)0);
		} else {
			i = statb.st_mtime;
#ifdef ST_MTIMESPEC_TV_NSEC
			j = statb.st_mtimespec.tv_nsec/1000;
#endif
#ifdef ST_MTIMENSEC
			j = statb.st_mtimensec/1000;
#endif
		}
		Set_flag_value(&job->info,JOB_TIME,i);
		Set_flag_value(&job->info,JOB_TIME_USEC,j);
	}

	/* set up the control file information */
	Setup_cf_info( job, check_for_existence );

	/* set the class of the job */
	if( !Find_str_value(&job->info,CLASS,Value_sep)
		&& (s = Find_str_value(&job->info,PRIORITY,Value_sep)) ){
		Set_str_value(&job->info,CLASS,s);
	}
	/* set the size of the job */
	if( !Find_flag_value(&job->info,SIZE,Value_sep) ){
		size = 0;
		for( i = 0; i < job->datafiles.count; ++i ){
			lp = (void *)job->datafiles.list[i];
			size +=  Find_flag_value(lp,SIZE,Value_sep);
		}
		Set_decimal_value(&job->info,SIZE,size);
	}

	Make_identifier( job );
	Check_for_hold( job, spool_control );

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3)Dump_job("Setup_job",job);
#endif
}
#endif

/* Get_hold_class( spool_control, job )
 *  check to see if the spool class and the job class are compatible
 *  returns:  non-zero if held, 0 if not held
 *   i.e.- cmpare(spoolclass,jobclass)
 */

int Get_hold_class( struct line_list *info, struct line_list *sq )
{
	int held, i;
	char *s, *t;
	struct line_list l;

	Init_line_list(&l);
	held = 0;
	if( (s = Clsses(sq))
		&& (t = Find_str_value(info,CLASS,Value_sep)) ){
		held = 1;
		Free_line_list(&l);
		Split(&l,s,File_sep,0,0,0,0,0,0);
		for( i = 0; held && i < l.count; ++i ){
			held = Globmatch( l.list[i], t );
		}
		Free_line_list(&l);
	}
	return(held);
}

/*
 * Extract the control file and data file information from the
 * control file image
 *
 * Note: we also handle HP extensions here.
 * 
 *  1. 4-Digit Job Numbers
 *  ----------------------
 *        
 *  HP preserves the System V-style 4-digit sequence number, or job number, in
 *  file names and attributes, while BSD uses 3-digit job numbers.
 * 
 *  
 *  2. Control and Data File Naming Conventions
 *  -------------------------------------------
 *     
 *  Control files are named in the following format:
 *        
 *     cA<seqn><host>
 *        
 *     <seqn> is the 4-digit sequence number (aka job number).
 *     <host> is the originating host name.
 * 
 *  The data file naming sequence format is:
 * 
 *      dA<seqn><host>   through   dZ<seqn><host>     followed by...
 *      da<seqn><host>   through   dz<seqn><host>     followed by...
 *      eA<seqn><host>   through   eZ<seqn><host>     followed by...
 *      ea<seqn><host>   through   ez<seqn><host>     ... etc. ...
 * 
 * 
 *  So the first data file name in a request begins with "dA", the second with
 *  "dB", the 27th with "da", the 28th with "db", and so forth.
 * 
 *
 * 3. HP-Specific BSD Job Attributes (Control File Lines)
 *  ------------------------------------------------------
 * 
 *  The following control file lines are extensions of RFC-1179:
 * 
 *     R<login>
 * 
 *        Write to the named login's terminal when the job is complete.  This is
 *        an alternate to the RFC-1179-style e-mail completion notification.
 *        This notification is selected via the lp "-w" option.
 * 
 *     A<priority>
 * 
 *        Specifies the System V-style priority of the request, a single digit
 *        from 0-7.
 * 
 *     N B<banner>
 * 
 *        Note that this line begins with an "N", a space, and then a "B".  The
 *        argument is the banner page title requested via the lp "-t" option.  If
 *        that option was not given then the argument is null.
 * 
 *     N O<options>
 * 
 *        Note that this line begins with an "N", a space, and then an "O".  The
 *        argument contains the System V-style "-o" options specified in the lp
 *        command line.  The option names appear without a leading "-o".  The
 *        first option name begins in the fourth character of the line; each
 *        option is separated by a blank.  If no "-o" options were given then the
 *        argument is null.
 *
 */

#if defined(JY20031104Append_Z_value)
void Append_Z_value( struct job *job, char *s )
{
	char *t;

	/* check for empty string */
	if( !s || !*s ) return;
	t = Find_str_value(&job->info,"Z",Value_sep);
	if( t && *t ){
		t = safestrdup3(t,",",s,__FILE__,__LINE__);
		Set_str_value(&job->info,"Z",t);
		if( t ) free(t); t = 0;
	} else {
		Set_str_value(&job->info,"Z",s);
	}
}
#endif

#if defined(JY20031104Setup_cf_info)
int Setup_cf_info( struct job *job, int check_for_existence )
{
	char *s;
	int i, c, n, copies = 0, last_format = 0;
	struct line_list cf_line_list;
	struct line_list *datafile = 0;
	struct stat statb;
	char buffer[SMALLBUFFER], *t;
	char *file_found;
	char *names = 0;
	int returnstatus = 0;
	int hpformat;

	Init_line_list(&cf_line_list);
	names = 0;

	hpformat = Find_flag_value(&job->info,HPFORMAT,Value_sep);
	if( (s = Find_str_value(&job->info,DATAFILES,Value_sep)) ){
		Split(&cf_line_list,s,"\001",0,0,0,0,0,0);
	} else {
		s = Find_str_value(&job->info,OPENNAME,Value_sep);
		if( !s ) s = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		DEBUG3("Setup_cf_info: control file '%s', hpformat '%d'", s, hpformat );
		if( Get_file_image_and_split(s,0,0, &cf_line_list, Line_ends,0,0,0,0,0,0)
			 && check_for_existence ){
#ifdef ORIGINAL_DEBUG//JY@1020
			DEBUG3("Setup_cf_info: missing or empty control file '%s'", s );
			SNPRINTF(buffer,sizeof(buffer))
				"no control file %s - %s", s, Errormsg(errno) );
#endif
			Set_str_value(&job->info,ERROR,buffer);
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			returnstatus = 1;
			goto done;
		}
	}

	Free_listof_line_list( &job->datafiles );

	file_found = 0;
	datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
	memset(datafile,0,sizeof(datafile[0]));

	for( i = 0; i < cf_line_list.count; ++i ){
		s = cf_line_list.list[i];
		Clean_meta(s);
		c = cval(s);
		DEBUG3("Setup_cf_info: doing line '%s'", s );
		if( islower(c) ){
			t = s;
			while( (t = strpbrk(t," \t")) ) *t++ = '_';
			if( file_found && (safestrcmp(file_found,s+1) || last_format != c) ){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}

			last_format = c;
			buffer[0] = c; buffer[1] = 0;
			Set_str_value(datafile,FORMAT,buffer);

			++copies;
			Set_flag_value(datafile,COPIES,copies);

			Set_str_value(datafile,TRANSFERNAME,s+1);
			Set_str_value(datafile,OPENNAME,s+1);
			file_found = Find_str_value(datafile,TRANSFERNAME,Value_sep);
			DEBUG4("Setup_cf_info: doing file '%s', format '%c', copies %d",
				file_found, last_format, copies );

			/* now we check for the status */
			if( check_for_existence ){
				if( stat(file_found, &statb) == 0 ){
					double size;
					size = statb.st_size;
					DEBUG4("Setup_cf_info: '%s' - size %0.0f", file_found, size );
					Set_double_value(datafile,SIZE,size );
				} else {
#ifdef ORIGINAL_DEBUG//JY@1020
					SNPRINTF(buffer,sizeof(buffer))
						"missing data file %s - %s", file_found, Errormsg(errno) );
#endif
					Set_str_value(&job->info,ERROR,buffer);
					Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
					returnstatus = 1;
					goto done;
				}
			}
		} else if( c == 'N' ){
			if( hpformat && cval(s+1) == ' '){
				/* this is an HP Format option */
				/* N B<banner> -> 'T' line */
				/* N Ooption option option-> prefix to Z */
				c = cval(s+2);
				if( c == 'B' ){
					if( s[3] ) Set_str_value(&job->info,"T",s+3);
				} else if( c == 'O' ){
					s = s+3;
					if( safestrlen(s) ){
						for( t = s; (t = strpbrk(t," ")); ++t ){
							*t = ',';
						}
						Append_Z_value(job,s);
					}
				}
				continue;
			}
			/* if we have a file name AND an 'N' for it, then set up a new file */
			if( file_found && (t = Find_str_value(datafile,"N",Value_sep))
				/* && safestrcmp(t,s+1) */ ){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}
			Set_str_value(datafile,"N",s+1);
			if( !names ){
				names = safestrdup(s+1,__FILE__,__LINE__);
			} else {
				names =  safeextend3(names,",",s+1,__FILE__,__LINE__);
			}
		} else if( c == 'U' ){
			Set_str_value(datafile,"U",s+1);
		} else {
			if( hpformat && c == 'Z' ){
				Append_Z_value( job, s+1 );
			} else if( hpformat && c == 'R' ){
				Set_str_value(&job->info,"M",s+1);
			} else if( hpformat && c == 'A' ){
				n = strtol( s+1,0,10);
				if( n >= 0 && n <=10){
					c = n + 'A';
					buffer[0] = n + 'A';
					buffer[1] = 0;
					Set_str_value(&job->info,PRIORITY,buffer);
				}
			} else if( isupper(c) ){
				buffer[0] = c; buffer[1] = 0;
				DEBUG4("Setup_cf_info: control '%s'='%s'", buffer, s+1 );
				Set_str_value(&job->info,buffer,s+1);
			}
		}
	}
	if( file_found ){
		Check_max(&job->datafiles,1);
		job->datafiles.list[job->datafiles.count++] = (void *)datafile;
		datafile = 0;
	} else {
		free(datafile);
		datafile = 0;
	}
	Set_str_value(&job->info,FILENAMES,names);

 done:

	if( datafile )	free(datafile); datafile=0;
	if( names )	free(names); names=0;
	Free_line_list( &cf_line_list );
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Setup_cf_info - final",job);
#endif
	return(returnstatus);
}
#endif

char *Make_hf_image( struct job *job )
{
	char *outstr, *s;
	int i;
	int len = safestrlen(OPENNAME);

	outstr = 0;
	for( i = 0; i < job->info.count; ++i ){
		s = job->info.list[i];
		if( !ISNULL(s) && !safestrpbrk(s,Line_ends) &&
			safestrncasecmp(OPENNAME,s,len) ){
			outstr = safeextend3(outstr,s,"\n",__FILE__,__LINE__);
		}
	}
	return( outstr );
}

/*
 * Write a hold file
 */

int Set_hold_file( struct job *job, struct line_list *perm_check, int fd )
{
	char *hf_name, *tempfile, *outstr;
	int status;

	status = 0;
	outstr = 0;

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Set_hold_file",job);
#endif
	Set_str_value(&job->info,UPDATE_TIME,Time_str(0,0));
	if( !(hf_name = Find_str_value(&job->info,HF_NAME,Value_sep)) ){
		Errorcode = JABORT;
		FATAL(LOG_ERR)"Set_hold_file: LOGIC ERROR- no HF_NAME in job information");
	}
	outstr = Make_hf_image( job );
	if( !fd ){
		fd = Make_temp_fd( &tempfile );
		if( Write_fd_str(fd, outstr) < 0 ){
			LOGERR(LOG_INFO)"Set_hold_file: write to '%s' failed", tempfile );
			status = 1;
		}
		close(fd);
		if( status == 0 && rename( tempfile, hf_name ) == -1 ){
			LOGERR(LOG_INFO)"Set_hold_file: rename '%s' to '%s' failed",
				tempfile, hf_name );
			status = 1;
		}
	} else {
		if( lseek( fd, 0, SEEK_SET ) == -1 ){
			LOGERR_DIE(LOG_ERR) "Set_hold_file: lseek failed" );
		}
		if( ftruncate( fd, 0 ) ){
			LOGERR_DIE(LOG_ERR) "Set_hold_file: ftruncate failed" );
		}
		if( Write_fd_str(fd, outstr) < 0 ){
			LOGERR(LOG_INFO)"Set_hold_file: write to '%s' failed", hf_name );
			status = 1;
		}
		/* close(fd); */
	}

	if( Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN );
	}

	/* we do this when we have a logger */
	if( status == 0 && Logger_fd > 0 ){
		char *t, *u;
		if( perm_check ){
			u = Join_line_list( perm_check, "\n" );
			t = Escape(u,1);
			outstr = safeextend5(outstr,"\n",LPC,"=",u,__FILE__,__LINE__);
			if(u) free(u); u = 0;
			if(t) free(t); t = 0;
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		send_to_logger(-1, -1, job,UPDATE,outstr);
#endif
	}
	if( outstr ) free( outstr ); outstr = 0;
	return( status );
}

/*
 * Get_hold_file( struct job *job, char *hf_name )
 *
 *  get hold file contents and initialize job->info hash with them
 */

void Get_hold_file( struct job *job, char *hf_name )
{
	char *s;
	if( (s = safestrchr(hf_name, '=')) ){
		hf_name = s+1;
	}
	DEBUG1("Get_hold_file: checking on '%s'", hf_name );

	Get_file_image_and_split( hf_name, 0, 0,
		&job->info, Line_ends, 1, Value_sep,1,1,1,0);
	if( !Find_str_value(&job->info,HF_NAME,Value_sep) ){
		Set_str_value(&job->info,HF_NAME,hf_name);
	}
}

/*
 * Get Spool Control Information
 *  - simply read the file
 */

void Get_spool_control( const char *file, struct line_list *info )
{
	Free_line_list(info);
	DEBUG2("Get_spool_control:  file '%s'", file );
	Get_file_image_and_split( file, 0, 0,
			info,Line_ends,1,Value_sep,1,1,1,0);
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Get_spool_control- info", info );
#endif
}

/*
 * Set Spool Control Information
 *  - simply write the file
 */

void Set_spool_control( struct line_list *perm_check, const char *file,
	struct line_list *info )
{
	char *s, *t, *tempfile;
	struct line_list l;
	int fd;

	s = t = tempfile = 0;
	Init_line_list(&l);
	fd = Make_temp_fd( &tempfile );
	DEBUG2("Set_spool_control: file '%s', tempfile '%s'",
		file, tempfile );
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_line_list("Set_spool_control- info", info );
#endif
	s = Join_line_list(info,"\n");
	if( Write_fd_str(fd, s) < 0 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Set_spool_control: cannot write tempfile '%s'",
			tempfile );
	}
	close(fd);
	if( rename( tempfile, file ) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Set_spool_control: rename of '%s' to '%s' failed",
			tempfile, file );
	}
	/* force and update of the cached status */

	if( Lpq_status_file_DYN ){
		/* do not check to see if this works */
		unlink(Lpq_status_file_DYN);
	}

	if( Logger_fd ){
		/* log the spool control file changes */
		t = Escape(s,1);
		Set_str_value(&l,QUEUE,t);
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;

		if( perm_check ){
			s = Join_line_list( perm_check, "\n" );
			t = Escape(s,1);
			Set_str_value(&l,LPC,t);
			if(s) free(s); s = 0;
			if(t) free(t); t = 0;
		}
		t = Join_line_list( &l, "\n");

#ifdef ORIGINAL_DEBUG//JY@1020
		send_to_logger(-1,-1,0,QUEUE,t);
#endif
	}

	Free_line_list(&l);
	if(s) free(s); s = 0;
	if(t) free(t); t = 0;
}

void intval( const char *key, struct line_list *list, struct job *job )
{
	int i = Find_flag_value(list,key,Value_sep);
	int len = safestrlen(job->sort_key);
	SNPRINTF(job->sort_key+len,sizeof(job->sort_key)-len)
    "|%s.0x%08x",key,i&0xffffffff);
	DEBUG5("intval: '%s'", job->sort_key );
}

void revintval( const char *key, struct line_list *list, struct job *job )
{
	int i = Find_flag_value(list,key,Value_sep);
	int len = safestrlen(job->sort_key);
	SNPRINTF(job->sort_key+len,sizeof(job->sort_key)-len)
	"|%s.0x%08x",key,(~i)&0xffffffff);
	DEBUG5("revintval: '%s'", job->sort_key );
}

void strzval( const char *key, struct line_list *list, struct job *job )
{
	char *s = Find_str_value(list,key,Value_sep);
	int len = safestrlen(job->sort_key);
	SNPRINTF(job->sort_key+len,sizeof(job->sort_key)-len)
	"|%s.%d",key,s!=0);
	DEBUG5("strzval: '%s'", job->sort_key );
}

void strnzval( const char *key, struct line_list *list, struct job *job )
{
	char *s = Find_str_value(list,key,Value_sep);
	int len = safestrlen(job->sort_key);
	SNPRINTF(job->sort_key+len,sizeof(job->sort_key)-len)
	"|%s.%d",key,(s==0 || *s == 0));
	DEBUG5("strnzval: '%s'", job->sort_key );
}

void strval( const char *key, struct line_list *list, struct job *job,
	int reverse )
{
	char *s = Find_str_value(list,key,Value_sep);
	int len = safestrlen(job->sort_key);
	int c = 0;

	if(s) c = cval(s);
	if( reverse ) c = -c;
	c = 0xFF & (-c);
	SNPRINTF(job->sort_key+len,sizeof(job->sort_key)-len)
	"|%s.%02x",key,c);
	DEBUG5("strval: '%s'", job->sort_key );
}


/*
 * Make_sort_key
 *   Make a sort key from the image information
 */
void Make_sort_key( struct job *job )
{
	job->sort_key[0] = 0;
	if( Order_routine_DYN ){
#if defined(ORDER_ROUTINE)
		extern char *ORDER_ROUTINE( struct job *job );
		ORDER_ROUTINE( &job );
#else
		Errorcode = JABORT;
		FATAL(LOG_ERR)"Make_sort_key: order_routine requested and ORDER_ROUTINE undefined");
#endif
	} else {
		/* first key is REMOVE_TIME - remove jobs come last */
		intval(REMOVE_TIME,&job->info,job);
#if 0
		/* first key is DONE_TIME - done jobs come last */
		intval(DONE_TIME,&job->info,job);
		intval(INCOMING_TIME,&job->info,job);
		/* next key is ERROR - error jobs jobs come before removed */
		intval(ERROR_TIME,&job->info,job);
#endif
		/* next key is HOLD - before the error jobs  */
		intval(HOLD_CLASS,&job->info,job);
		intval(HOLD_TIME,&job->info,job);
		/* next key is MOVE - before the held jobs  */
		strnzval(MOVE,&job->info,job);
		/* now by priority */
		if( Ignore_requested_user_priority_DYN == 0 ){
			strval(PRIORITY,&job->info,job,Reverse_priority_order_DYN);
		}
		/* now we do TOPQ */
		revintval(PRIORITY_TIME,&job->info,job);
		/* now we do FirstIn, FirstOut */
		intval(JOB_TIME,&job->info,job);
		intval(JOB_TIME_USEC,&job->info,job);
		/* now we do by job number if two at same time (very unlikely) */
		intval(NUMBER,&job->info,job);
	}
}
#ifdef REMOVE

/*
 * Set up printer
 *  1. reset configuration information
 *  2. check the printer name
 *  3. get the printcap information
 *  4. set the configuration variables
 *  5. If run on the server,  then check for the Lp_device_DYN
 *     being set.  If it is set, then clear the RemotePrinter_DYN
 *     and RemoteHost_DYN.
 */

#if defined(JY20031104Setup_printer)
int Setup_printer( char *prname, char *error, int errlen, int subserver )
{
	char *s;
	int status = 0;
	char name[SMALLBUFFER];

	DEBUG3( "Setup_printer: checking printer '%s'", prname );

	/* reset the configuration information, just in case
	 * this is a subserver or being used to get status
	 */
	safestrncpy(name,prname);
	if( error ) error[0] = 0;
	if( (s = Is_clean_name( name )) ){
		SNPRINTF( error, errlen)
			"printer '%s' has illegal char at '%s' in name", name, s );
		status = 1;
		goto error;
	}
	lowercase(name);
	if( !subserver && Status_fd > 0 ){
		close( Status_fd );
		Status_fd = -1;
	}
	Set_DYN(&Printer_DYN,name);
	Fix_Rm_Rp_info(0,0);

	if( Spool_dir_DYN == 0 || *Spool_dir_DYN == 0 ){
		SNPRINTF( error, errlen)
"spool queue for '%s' does not exist on server %s\n   non-existent printer or you need to run 'checkpc -f'",
			name, FQDNHost_FQDN );
		status = 2;
		goto error;
	}

	if( chdir( Spool_dir_DYN ) < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error, errlen)
			"printer '%s', chdir to '%s' failed '%s'",
				name, Spool_dir_DYN, Errormsg( errno ) );
#endif
		status = 2;
		goto error;
	}

	/*
	 * get the override information from the control/spooling
	 * directory
	 */

	Get_spool_control( Queue_control_file_DYN, &Spool_control );

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list,
			Printer_DYN );
	}

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUG1("Setup_printer: printer now '%s', spool dir '%s'",
		Printer_DYN, Spool_dir_DYN );
	if(DEBUGL3){
		Dump_parms("Setup_printer - vars",Pc_var_list);
		Dump_line_list("Setup_printer - spool control", &Spool_control );
	}
#endif

 error:
	DEBUG3("Setup_printer: status '%d', error '%s'", status, error );
	return( status );
}
#endif

/**************************************************************************
 * Read_pid( int fd, char *str, int len )
 *   - Read the pid from a file
 **************************************************************************/
int Read_pid( int fd, char *str, int len )
{
	char line[LINEBUFFER];
	int n;

	if( lseek( fd, 0, SEEK_SET ) == -1 ){
		LOGERR_DIE(LOG_ERR) "Read_pid: lseek failed" );
	}

	if( str == 0 ){
		str = line;
		len = sizeof( line );
	}
	str[0] = 0;
	if( (n = read( fd, str, len-1 ) ) < 0 ){
		LOGERR_DIE(LOG_ERR) "Read_pid: read failed" );
	}
	str[n] = 0;
	n = atoi( str );
	DEBUG3( "Read_pid: %d", n );
	return( n );
}

/**************************************************************************
 * Write_pid( int fd )
 *   - Write the pid to a file
 **************************************************************************/
int Write_pid( int fd, int pid, char *str )
{
	char line[LINEBUFFER];

	if( lseek( fd, 0, SEEK_SET ) == -1 ){
		LOGERR(LOG_ERR) "Write_pid: lseek failed" );
		return -1;
	}
	if( ftruncate( fd, 0 ) ){
		LOGERR(LOG_ERR) "Write_pid: ftruncate failed" );
		return -1;
	}

	if( str == 0 ){
		SNPRINTF( line, sizeof(line)) "%d\n", pid );
	} else {
		SNPRINTF( line, sizeof(line)) "%s\n", str );
	}
	DEBUG3( "Write_pid: pid %d, str '%s'", pid, str );
	if( Write_fd_str( fd, line ) < 0 ){
		LOGERR(LOG_ERR) "Write_pid: write failed" );
		return -1;
	}
	return 0;
}
#endif
/***************************************************************************
 * int Patselect( struct line_list *tokens, struct line_list *cf );
 *    check to see that the token value matches one of the following
 *    in the control file:
 *  token is INTEGER: then matches the job number
 *  token is string: then matches either the user name or host name
 *    then try glob matching job ID
 *  return:
 *   0 if match found
 *   nonzero if not match found
 ***************************************************************************/

int Patselect( struct line_list *token, struct line_list *cf, int starting )
{
	int match = 1;
	int i, n, val;
	char *key, *s, *end;
	
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3)Dump_line_list("Patselect- tokens", token );
	if(DEBUGL3)Dump_line_list("Patselect- info", cf );
#endif
	for( i = starting; match && i < token->count; ++i ){
		key = token->list[i];
		DEBUG3("Patselect: key '%s'", key );
		/* handle wildcard match */
		if( !(match = safestrcasecmp( key, "all" ))){
			break;
		}
		end = key;
		val = strtol( key, &end, 10 );
		if( *end == 0 ){
			n = Find_decimal_value(cf,NUMBER,Value_sep);
			/* we check job number */
			DEBUG3("Patselect: job number check '%d' to job %d",
				val, n );
			match = (val != n);
		} else {
			/* now we check to see if we have a name match */
			if( (s = Find_str_value(cf,LOGNAME,Value_sep))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
			if( (s = Find_str_value(cf,IDENTIFIER,Value_sep))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
		}
	}
	DEBUG3("Patselect: returning %d", match);
	return(match);
}

/***************************************************************************
 * char * Check_format( int type, char *name, struct control_file *job )
 * Check to see that the file name has the correct format
 * name[0] == 'c' or 'd' (type)
 * name[1] = 'f'
 * name[2] = A-Za-z
 * name[3-5] = NNN
 * name[6-end] = only alphanumeric and ., _, or - chars
 * RETURNS: 0 if OK, error message (string) if not
 *
 * Summary of HP's Extensions to RFC-1179
 * 1. 4-Digit Job Numbers
 *  ----------------------
 *  HP preserves the System V-style 4-digit sequence number, or job number, in
 *  file names and attributes, while BSD uses 3-digit job numbers.
 *  2. Control and Data File Naming Conventions
 *  -------------------------------------------
 *  Control files are named in the following format:
 *     cA<seqn><host>
 *     <seqn> is the 4-digit sequence number (aka job number).
 *     <host> is the originating host name.
 *  The data file naming sequence format is:
 *      dA<seqn><host>   through   dZ<seqn><host>     followed by...
 *      da<seqn><host>   through   dz<seqn><host>     followed by...
 *      eA<seqn><host>   through   eZ<seqn><host>     followed by...
 *      ea<seqn><host>   through   ez<seqn><host>     ... etc. ...
 *  So the first data file name in a request begins with "dA", the second with
 *  "dB", the 27th with "da", the 28th with "db", and so forth.

 ***************************************************************************/
#if defined(JY20031104Check_format)
int Check_format( int type, const char *name, struct job *job )
{
	int n, c, hpformat;
	const char *s;
	char *t;
	char msg[SMALLBUFFER];

	DEBUG4("Check_format: type %d, name '%s'", type, name ); 
	msg[0] = 0;
	hpformat = 0;
	n = cval(name);
	switch( type ){
	case DATA_FILE:
		if( n != 'd' ){
			SNPRINTF(msg, sizeof(msg))
				"data file does not start with 'd' - '%s'",
				name );
			goto error;
		}
		break;
	case CONTROL_FILE:
		if( n != 'c' ){
			SNPRINTF(msg, sizeof(msg))
				"control file does not start with 'c' - '%s'",
				name );
			goto error;
		}
		break;
	default:
		SNPRINTF(msg, sizeof(msg))
			"bad file type '%c' - '%s' ", type,
			name );
		goto error;
	}
	/* check for second letter */
	n = cval(name+1);
	if( n == 'A' ){
		/* HP format */
		hpformat = 1;
	} else if( n != 'f' ){
		SNPRINTF(msg, sizeof(msg))
			"second letter must be f not '%c' - '%s' ", n, name );
		goto error;
	} else {
		n = cval(name+2);
		if( !isalpha( n ) ){
			SNPRINTF(msg, sizeof(msg))
				"third letter must be letter not '%c' - '%s' ", n, name );
			goto error;
		}
    }
	if( type == CONTROL_FILE ){
		SNPRINTF(msg,sizeof(msg))"%c",n);
		Set_str_value(&job->info,PRIORITY,msg);
		msg[0] = 0;
	}
	/*
		we now enter the wonderful world of 'conventions'
		cfAnnnHostname
              ^^^^ starts with letter (number len = 0, 1, 2, 3)
		cfAnnnIPV4.Add.ress  (number len = 4, 5, ... )
              ^^^^ starts with number or is a 3com type thing
		cfAnnnnnnHostName    (len = 6)
                 ^^^^ starts with letter
		cfAnnnnnnIPV4.Add.ress  (len = 7, ... )
                 ^^^^ starts with number
	*/
   
	if( hpformat ){
		/* we have four digits */
		safestrncpy(msg,&name[2]);
		t = 0;
		n = strtol(msg,&t,10);
	} else {
		safestrncpy(msg,&name[3]);
		for( t = msg; isdigit(cval(t)); ++t );
		c = t - msg;
		switch( c ){
		case 0: case 1: case 2: case 3:
			break;
		case 4: case 5:
			c = 3;
			break;
		default:
			if( cval(msg+6) == '.' ) c = 3;
			else c = 6;
			break;
		}
		/* get the number */
		t = &msg[c];
		c = *t;
		*t = 0;
		n = strtol(msg,0,10);
		*t = c;
    }

	DEBUG1("Check_format: name '%s', number %d, file '%s'", name, n, t ); 
	if( Find_str_value( &job->info,NUMBER,Value_sep) ){
		c = Find_decimal_value( &job->info,NUMBER,Value_sep);
		if( c != n ){
			SNPRINTF(msg, sizeof(msg))
				"job numbers differ '%s', old %d and new %d",
					name, c, n );
			goto error;
		}
	} else {
		Fix_job_number( job, n );
	}
	Clean_name(t);
	if( (s = Find_str_value( &job->info,FILE_HOSTNAME,Value_sep)) ){
		if( safestrcasecmp(s,t) ){
			SNPRINTF(msg, sizeof(msg))
				"bad hostname '%s' - '%s' ", t, name );
			goto error;
		}
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,t);
	}
	/* clear out error message */
	msg[0] = 0;

 error:
	if( hpformat ){
		Set_flag_value(&job->info,HPFORMAT,hpformat);
	}
	if( msg[0] ){
		DEBUG1("Check_format: %s", msg ); 
		Set_str_value(&job->info,FORMAT_ERROR,msg);
	}
	return( msg[0] != 0 );
}
#endif

char *Find_start(char *str, const char *key )
{
	int n = safestrlen(key);
	while( (str = strstr(str,key)) && str[n] != '=' );
	if( str ) str += (n+1);
	return( str );
}

char *Frwarding(struct line_list *l)
{
	return( Find_str_value(l,FORWARDING,Value_sep) );
}
int Pr_disabled(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_DISABLED,Value_sep) );
}
int Sp_disabled(struct line_list *l)
{
	return( Find_flag_value(l,SPOOLING_DISABLED,Value_sep) );
}
int Pr_aborted(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_ABORTED,Value_sep) );
}
int Hld_all(struct line_list *l)
{
	return( Find_flag_value(l,HOLD_ALL,Value_sep) );
}
char *Clsses(struct line_list *l)
{
	return( Find_str_value(l,CLASS,Value_sep) );
}
char *Cntrol_debug(struct line_list *l)
{
	return( Find_str_value(l,DEBUG,Value_sep) );
}
char *Srver_order(struct line_list *l)
{
	return( Find_str_value(l,SERVER_ORDER,Value_sep) );
}

/*
 * Job datastructure management
 */

void Init_job( struct job *job )
{
	memset(job,0,sizeof(job[0]) );
}

void Free_job( struct job *job )
{
	Free_line_list( &job->info );
	Free_listof_line_list( &job->datafiles );
	Free_line_list( &job->destination );
}

void Copy_job( struct job *dest, struct job *src )
{
	Merge_line_list( &dest->info, &src->info, 0,0,0 );
	Merge_listof_line_list( &dest->datafiles, &src->datafiles, 0,0,0 );
	Merge_line_list( &dest->destination, &src->destination, 0,0,0 );
}

/**************************************************************************
 * static int Fix_job_number();
 * - fixes the job number range and value
 **************************************************************************/

char *Fix_job_number( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	int len = 3, max = 1000;

	if( n == 0 ){
		n = Find_decimal_value( &job->info, NUMBER, Value_sep );
	}
	if( Long_number_DYN && !Backwards_compatible_DYN ){
		len = 6;
		max = 1000000;
	}
	SNPRINTF(buffer,sizeof(buffer))"%0*d",len, n % max );
	Set_str_value(&job->info,NUMBER,buffer);
	return( Find_str_value(&job->info,NUMBER,Value_sep) );
}

/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format name@host%id
 *  It is put in the 'A' field on the name.
 * 
 ************************************************************************/

#if defined(JY20031104Make_identifier)
char *Make_identifier( struct job *job )
{
	char *user, *host, *s, *id;
	char number[32];
	int n;

	if( !(s = Find_str_value( &job->info,IDENTIFIER,Value_sep )) ){
		if( !(user = Find_str_value( &job->info,"P",Value_sep ))){
			user = "nobody";
		}
		if( !(host= Find_str_value( &job->info,"H",Value_sep ))){
			host = "unknown";
		}
		n = Find_decimal_value( &job->info,NUMBER,Value_sep );
		SNPRINTF(number,sizeof(number))"%d",n);
		if( (s = safestrchr( host, '.' )) ) *s = 0;
		id = safestrdup5(user,"@",host,"+",number,__FILE__,__LINE__);
		if( s ) *s = '.';
		Set_str_value(&job->info,IDENTIFIER,id);
		if( id ) free(id); id = 0;
		s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	}
	return(s);
}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
void Dump_job( char *title, struct job *job )
{
	int i;
	struct line_list *lp;
	if( title ) LOGDEBUG( "*** Job %s *** - 0x%lx", title, Cast_ptr_to_long(job));
#ifdef ORIGINAL_DEBUG//JY@1020
	Dump_line_list_sub( "info",&job->info);
	LOGDEBUG("  datafiles - count %d", job->datafiles.count );
#endif
	for( i = 0; i < job->datafiles.count; ++i ){
		char buffer[SMALLBUFFER];
		SNPRINTF(buffer,sizeof(buffer))"  datafile[%d]", i );
		lp = (void *)job->datafiles.list[i];
#ifdef ORIGINAL_DEBUG//JY@1020
		Dump_line_list_sub(buffer,lp);
#endif
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	Dump_line_list_sub( "destination",&job->destination);
#endif
	if( title ) LOGDEBUG( "*** end ***" );
}
#endif

#if defined(JY20031104Job_printable)
void Job_printable( struct job *job, struct line_list *spool_control,
	int *pprintable, int *pheld, int *pmove, int *perr, int *pdone )
{
	char *s;
	char buffer[SMALLBUFFER];
	char destbuffer[SMALLBUFFER];
	struct stat statb; 
	int n, printable = 0, held = 0, move = 0, error = 0, done = 0,destination, destinations;

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Job_printable - job info",job);
	if(DEBUGL4)Dump_line_list("Job_printable - spool control",spool_control);
#endif

	buffer[0] = 0;
	if( job->info.count == 0 ){
		SNPRINTF(buffer,sizeof(buffer)) "removed" );
	} else if( Find_flag_value(&job->info,INCOMING_TIME,Value_sep) ){
		SNPRINTF(buffer,sizeof(buffer)) "incoming" );
	} else if( (error = Find_flag_value(&job->info,ERROR_TIME,Value_sep)) ){
		SNPRINTF(buffer,sizeof(buffer)) "error" );
	} else if( Find_flag_value(&job->info,HOLD_TIME,Value_sep) ){
		SNPRINTF(buffer,sizeof(buffer)) "hold" );
		held = 1;
	} else if( (done = Find_flag_value(&job->info,DONE_TIME,Value_sep)) ){
		SNPRINTF(buffer,sizeof(buffer)) "done" );
	} else if( (n = Find_flag_value(&job->info,SERVER,Value_sep))
		&& kill( n, 0 ) == 0 ){
		int delta;
		time_t now = time((void *)0);
		time_t last_change = Find_flag_value(&job->info,START_TIME,Value_sep);
		if( !ISNULL(Status_file_DYN) && !stat( Status_file_DYN, &statb )
			&& last_change < statb.st_mtime ){
			last_change = statb.st_mtime;
		}
		if( !ISNULL(Log_file_DYN) && !stat( Log_file_DYN, &statb )
			&& last_change < statb.st_mtime ){
			last_change = statb.st_mtime;
		}
		delta = now - last_change;
		if( Stalled_time_DYN && delta > Stalled_time_DYN ){
			SNPRINTF( buffer, sizeof(buffer))
				"stalled(%dsec)", delta );
		} else {
			n = Find_flag_value(&job->info,ATTEMPT,Value_sep);
			SNPRINTF(buffer,sizeof(buffer)) "active" );
			if( n > 0 ){
				SNPRINTF( buffer, sizeof(buffer))
					"active(attempt-%d)", n+1 );
			}
		}
		printable = 1;
	} else if((s = Find_str_value(&job->info,MOVE,Value_sep)) ){
		SNPRINTF(buffer,sizeof(buffer)) "moved->%s", s );
		move = 1;
	} else if( Get_hold_class(&job->info, spool_control ) ){
		SNPRINTF(buffer,sizeof(buffer)) "holdclass" );
		held = 1;
	} else {
		printable = 1;
	}
	if( (destinations = Find_flag_value(&job->info,DESTINATIONS,Value_sep)) ){
		printable = 0;
		for( destination = 0; destination < destinations; ++destination ){
			Get_destination(job,destination);
#ifdef ORIGINAL_DEBUG//JY@1020
			if(DEBUGL4)Dump_job("Job_destination_printable - job",job);
#endif
			destbuffer[0] = 0;
			if( Find_flag_value(&job->destination,ERROR_TIME,Value_sep) ){
				SNPRINTF(destbuffer,sizeof(destbuffer)) "error" );
			} else if( Find_flag_value(&job->destination,HOLD_TIME,Value_sep) ){
				SNPRINTF(destbuffer,sizeof(destbuffer)) "hold" );
				held += 1;
			} else if( Find_flag_value(&job->destination,DONE_TIME,Value_sep) ){
				SNPRINTF(destbuffer,sizeof(destbuffer)) "done" );
			} else if( (n = Find_flag_value(&job->destination,SERVER,Value_sep))
				&& kill( n, 0 ) == 0 ){
				int delta;
				n = Find_flag_value(&job->destination,START_TIME,Value_sep);
				delta = time((void *)0) - n;
				if( Stalled_time_DYN && delta > Stalled_time_DYN ){
					SNPRINTF( destbuffer, sizeof(destbuffer))
						"stalled(%dsec)", delta );
				} else {
					n = Find_flag_value(&job->destination,ATTEMPT,Value_sep);
					SNPRINTF(destbuffer,sizeof(destbuffer)) "active" );
					if( n > 0 ){
						SNPRINTF( destbuffer, sizeof(destbuffer))
							"active(attempt-%d)", n+1 );
					}
				}
				printable += 1;
			} else if((s = Find_str_value(&job->destination,MOVE,Value_sep)) ){
				SNPRINTF(destbuffer,sizeof(destbuffer)) "moved->%s", s );
				move += 1;
			} else if( Get_hold_class(&job->destination, spool_control ) ){
				SNPRINTF(destbuffer,sizeof(destbuffer)) "holdclass" );
				held += 1;
			} else {
				printable += 1;
			}
			Set_str_value(&job->destination,PRSTATUS,destbuffer);
			Set_flag_value(&job->destination,PRINTABLE,printable);
			Set_flag_value(&job->destination,HELD,held);
			Update_destination(job);
		}
	}

	Set_str_value(&job->info,PRSTATUS,buffer);
	Set_flag_value(&job->info,PRINTABLE,printable);
	Set_flag_value(&job->info,HELD,held);
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	if( perr ) *perr = error;
	if( pdone ) *pdone = done;
	DEBUG3("Job_printable: printable %d, held %d, move '%d', error '%d', done '%d', status '%s'",
		printable, held, move, error, done, buffer );
}
#endif

#ifdef REMOVE
int Server_active( char *file )
{
	struct stat statb;
	int serverpid = 0;
	int fd = Checkread( file, &statb );
	if( fd >= 0 ){
		serverpid = Read_pid( fd, 0, 0 );
		close(fd);
		DEBUG5("Server_active: checking file %s, serverpid %d", file, serverpid );
		if( serverpid && kill(serverpid,0) ){
			serverpid = 0;
		}
	}
	DEBUG3("Server_active: file %s, serverpid %d", file, serverpid );
	return( serverpid );
}
#endif

/*
 * Destination Information
 *   The destination information is stored in the control file
 * as lines of the form:
 * NNN=.....   where NNN is the destination number
 *                   .... is the escaped destination information
 * During normal printing or other activity,  the destination information
 * is unpacked into the job->destination line list.
 */

/*
 * Update_destination updates the information with the values in the
 *  job->destination line list.
 */
void Update_destination( struct job *job )
{
	char *s, *t, buffer[SMALLBUFFER];
	int id;
	id = Find_flag_value(&job->destination,DESTINATION,Value_sep);
	SNPRINTF(buffer,sizeof(buffer))"DEST%d",id);
	s = Join_line_list(&job->destination,"\n");
	t = Escape(s,1);
	Set_str_value(&job->info,buffer,t);
	free(s);
	free(t);
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Update_destination",job);
#endif
}

/*
 * Get_destination puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	char *s;
	int result = 1;

	SNPRINTF(buffer,sizeof(buffer)) "DEST%d", n );

	Free_line_list(&job->destination);
	if( (s = Find_str_value(&job->info,buffer,Value_sep)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Value_sep,1,1,1,0);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Get_destination_by_name puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination_by_name( struct job *job, char *name )
{
	int result = 1;
	char *s;

	Free_line_list(&job->destination);
	if( name && (s = Find_str_value(&job->info,name,Value_sep)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Value_sep,1,1,1,0);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Trim_status_file - trim a status file to an acceptible length
 */

int Trim_status_file( int status_fd, char *file, int max, int min )
{
	int tempfd, status;
	char buffer[LARGEBUFFER];
	struct stat statb;
	char *tempfile, *s;
	int count;

	tempfd = status = -1;

	DEBUG1("Trim_status_file: file '%s' max %d, min %d", file, max, min);

	/* if the file does not exist, do not create it */
	if( ISNULL(file) ) return( status_fd );
	if( stat( file, &statb ) == 0 ){
		DEBUG1("Trim_status_file: '%s' max %d, min %d, size %ld", file, max, min, 
			(long)(statb.st_size) );
		if( max > 0 && statb.st_size/1024 > max ){
			status = Checkwrite( file, &statb,O_RDWR,0,0);
			tempfd = Make_temp_fd(&tempfile);
			if( min > max || min == 0 ){
				min = max/4;
			}
			if( min == 0 ) min = 1;
			DEBUG1("Trim_status_file: trimming to %d K", min);
			lseek( status, 0, SEEK_SET );
			lseek( status, -min*1024, SEEK_END );
			while( (count = read( status, buffer, sizeof(buffer) - 1 ) ) > 0 ){
				buffer[count] = 0;
				if( (s = safestrchr(buffer,'\n')) ){
					*s++ = 0;
					Write_fd_str( tempfd, s );
					break;
				}
			}
			while( (count = read( status, buffer, sizeof(buffer) ) ) > 0 ){
				if( write( tempfd, buffer, count) < 0 ){
					Errorcode = JABORT;
					LOGERR_DIE(LOG_ERR) "Trim_status_file: cannot write tempfile" );
				}
			}
			lseek( tempfd, 0, SEEK_SET );
			lseek( status, 0, SEEK_SET );
			ftruncate( status, 0 );
			while( (count = read( tempfd, buffer, sizeof(buffer) ) ) > 0 ){
				if( write( status, buffer, count) < 0 ){
					Errorcode = JABORT;
					LOGERR_DIE(LOG_ERR) "Trim_status_file: cannot write tempfile" );
				}
			}
			unlink(tempfile);
			close(status);
		}
		close( tempfd );
		if( status_fd > 0 ) close( status_fd );
		status_fd = Checkwrite( file, &statb,0,0,0);
	}
	return( status_fd );
}

/********************************************************************
 * int Fix_control( struct job *job, char *order )
 *   fix the order of lines in the control file so that they
 *   are in the order of the letters in the order string.
 * Lines are checked for metacharacters and other trashy stuff
 *   that might have crept in by user efforts
 *
 * job - control file area in memory
 * order - order of options
 *
 *  order string: Letter - relative position in file
 *                * matches any character not in string
 *                  can have only one wildcard in string
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *
 * RETURNS: 0 if fixed correctly
 *          non-zero if there is something wrong with this file and it should
 *          be rejected out of hand
 ********************************************************************/

/********************************************************************
 * BSD and LPRng order
 * We use these values to determine the order of jobs in the file
 * The order of the characters determines the order of the options
 *  in the control file.  A * puts all unspecified options there
 ********************************************************************/

 static char BSD_order[] = "HPJCLIMWT1234" ;
 static char LPRng_order[] = "HPJCLIMWT1234*" ;

#if defined(JY20031104Fix_datafile_info)
char *Fix_datafile_info( struct job *job, char *number, char *suffix,
	char *xlate_format )
{
	int i, copies, linecount, count, jobcopies, copy, group, offset;
	char *s, *Nline, *transfername, *dataline, *jobline;
	struct line_list *lp, outfiles;
	char prefix[8];
	char fmt[2];
	
	Init_line_list(&outfiles);
	transfername = dataline = Nline = jobline = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Fix_datafile_info - starting", job );
#endif

	/* now we find the number of different data files */

	count = 0;
	/* we look through the data file list, looking for jobs with the name
	 * TRANSFERNAME.  If we find a new one, we create the correct form
	 * of the job datafile
	 */
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		lp = (void *)job->datafiles.list[linecount];
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		Set_str_value(lp,OTRANSFERNAME,transfername);
		if( !(s = Find_casekey_str_value(&outfiles,transfername,Value_sep)) ){
			/* we add the entry */
			offset = count % 52;
			group = count / 52;
			++count;
			if( (group >= 52) ){
				FATAL(LOG_INFO)"Fix_datafile_info: too many data files");
			}
			SNPRINTF(prefix,sizeof(prefix))"d%c%c",
			("fghijklmnopqrstuvwxyzabcde" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" )[group],
			("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz")[offset] );
			s = safestrdup3(prefix,number,suffix,__FILE__,__LINE__);
			if( transfername ) Set_casekey_str_value(&outfiles,transfername,s);
			Set_str_value(lp,TRANSFERNAME,s);
			if(s) free(s); s = 0;
		} else {
			Set_str_value(lp,TRANSFERNAME,s);
		}
	}
	Free_line_list(&outfiles);
	Set_decimal_value(&job->info,DATAFILE_COUNT,count);

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL4)Dump_job("Fix_datafile_info - after finding duplicates", job );
#endif

	jobcopies = Find_flag_value(&job->info,COPIES,Value_sep);
	if( !jobcopies ) jobcopies = 1;
	fmt[0] = 'f'; fmt[1] = 0;
	DEBUG4("Fix_datafile_info: jobcopies %d", jobcopies );
	for(copy = 0; copy < jobcopies; ++copy ){
		for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
			jobline = 0;
			lp = (void *)job->datafiles.list[linecount];
#ifdef ORIGINAL_DEBUG//JY@1020
			if(DEBUGL5)Dump_line_list("Fix_datafile_info - info", lp  );
#endif
			transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
			Nline = Find_str_value(lp,"N",Value_sep);
			fmt[0] = 'f';
			if( (s = Find_str_value(lp,FORMAT,Value_sep)) ){
				fmt[0] = *s;
			}
			if( xlate_format ){
				int l = safestrlen(xlate_format);
				for( i = 0; i+1 < l; i+= 2 ){
					if( (xlate_format[i] == fmt[0])
						|| (xlate_format[i] == '*') ){
						fmt[0] = xlate_format[i+1];
						break;
					}
				}
			}
			copies = Find_flag_value(lp,COPIES,Value_sep);
			if( copies == 0 ) copies = 1;
			for(i = 0; i < copies; ++i ){
				if( Nline && !Nline_after_file_DYN ){
					jobline = safeextend4(jobline,"N",Nline,"\n",__FILE__,__LINE__);
				}
				jobline = safeextend4(jobline,fmt,transfername,"\n",__FILE__,__LINE__);
				if( Nline && Nline_after_file_DYN ){
					jobline = safeextend4(jobline,"N",Nline,"\n",__FILE__,__LINE__);
				}
			}
			DEBUG4("Fix_datafile_info: file [%d], jobline '%s'",
				linecount, jobline);
			dataline = safeextend2(dataline,jobline,__FILE__,__LINE__);
			if( jobline ) free(jobline); jobline = 0;
		}
	}
	DEBUG4("Fix_datafile_info: adding remove lines" );
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		jobline = 0;
		lp = (void *)job->datafiles.list[linecount];
#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL4)Dump_line_list("Fix_datafile_info - info", lp );
#endif
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		if( !Find_casekey_str_value(&outfiles,transfername,Value_sep) ){
			jobline = safeextend4(jobline,"U",transfername,"\n",__FILE__,__LINE__);
			Set_casekey_str_value(&outfiles,transfername,"YES");
		}
		DEBUG4("Fix_datafile_info: file [%d], jobline '%s'",
			linecount, jobline);
		dataline = safeextend2(dataline,jobline,__FILE__,__LINE__);
		if( jobline ) free(jobline); jobline = 0;
	}
	Free_line_list(&outfiles);
	Set_str_value(&job->info,DATAFILES,dataline);
	s = Find_str_value(&job->info,DATAFILES,Value_sep);
	while( (s = safestrchr(s,'\n')) ) *s++ = '\001';
	return(dataline);
}
#endif

int ordercomp(  const void *left, const void *right, const void *orderp)
{
	const char *lpos, *rpos, *wildcard, *order;
	int cmp;

	order = (const char *)orderp;

	/* blank lines always come last */
	if( (wildcard = safestrchr( order, '*' )) ){
		wildcard = order + safestrlen(order);
	}
	lpos = *((const char **)left);
	if( lpos == 0 || *lpos == 0 ){
		lpos = order+safestrlen(order);
	} else if( !(lpos = safestrchr( order, *lpos )) ){
		lpos = wildcard;
	}
	rpos = *((const char **)right);
	if( rpos == 0 || *rpos == 0 ){
		rpos = order+safestrlen(order);
	} else if( !(rpos = safestrchr( order, *rpos )) ){
		rpos = wildcard;
	}
	cmp = lpos - rpos;
	DEBUG4("ordercomp '%s' to '%s' -> %d",
		*((const char **)left), *((const char **)right), cmp );
	return( cmp );
}

/************************************************************************
 * Fix_control:
 *  Fix up the control file,  setting the various entries
 *  to be compatible with transfer to the remote location
 * 1. info will have fromhost, priority, and number information
 *   if not, you will need to add it.
 *
 ************************************************************************/

 struct maxlen{
	int c, len;
 } maxclen[] = {
	{ 'A', 131 }, { 'C', 31 }, { 'D', 1024 }, { 'H', 31 }, { 'I', 31 },
	{ 'J', 99 }, { 'L', 31 }, { 'N', 131 }, { 'M', 131 }, { 'N', 131 },
	{ 'P', 31 }, { 'Q', 131 }, { 'R', 131 }, { 'S', 131 }, { 'T', 79 },
	{ 'U', 131 }, { 'W', 31 }, { 'Z', 1024 }, { '1', 131 }, { '2', 131 },
	{ '3', 131 }, { '4', 131 },
	{0,0}
	};

#ifdef ORIGINAL_DEBUG//JY@1020
void Fix_control( struct job *job, char *filter, char *xlate_format )
{
	char *s, *file_hostname, *number, *priority,
		*order;
	char buffer[SMALLBUFFER], pr[2];
	int tempfd, tempfc;
	int i, n, j, cccc, wildcard, len;
	struct line_list controlfile;

	Init_line_list(&controlfile);

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_job( "Fix_control: starting", job );
#endif

	/* we set the control file with the single letter values in the
	   hold job file
	 */
	for( i = 0; i < job->info.count; ++i ){
		int c;
		s = job->info.list[i];
		if( s && (c = s[0]) && isupper(c) && c != 'N' && c != 'U'
			&& s[1] == '=' ){
			/* remove banner from control file */
			if( c == 'L' && Suppress_header_DYN && !Always_banner_DYN ) continue;
			s[1] = 0;
			Add_line_list( &controlfile, s, Value_sep, 1, 1 );
			Set_str_value(&controlfile,s,s+2);
			s[1] = '=';
		}
	}

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_line_list( "Fix_control: control file", &controlfile );
#endif

	n = j = 0;
	n = Find_decimal_value( &job->info,NUMBER,Value_sep);
	j = Find_decimal_value( &job->info,SEQUENCE,Value_sep);

	number = Fix_job_number(job, n+j);
	
	if( !(priority = Find_str_value( &job->destination,PRIORITY,Value_sep))
		&& !(priority = Find_str_value( &job->info,PRIORITY,Value_sep))
		&& !(priority = Default_priority_DYN) ){
		priority = "A";
	}
	pr[0] = *priority;
	pr[1] = 0;

	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);

	if( !file_hostname ){
		file_hostname = Find_str_value(&job->info,FROMHOST,Value_sep);
		if( file_hostname == 0 || *file_hostname == 0 ){
			file_hostname = FQDNHost_FQDN;
		}
		Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
		file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);
	}

	if( (Backwards_compatible_DYN || Use_shorthost_DYN)
		&& (s = safestrchr( file_hostname, '.' )) ){
		*s = 0;
	}

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_job( "Fix_control: before fixing", job );
#endif

	/* fix control file name */

	s = safestrdup4("cf",pr,number,file_hostname,__FILE__,__LINE__);
	Set_str_value(&job->info,TRANSFERNAME,s);
	if(s) free(s); s = 0;

	/* fix control file contents */

	s = Make_identifier( job );

	if( job->destination.count == 0 ){
		Set_str_value(&controlfile,IDENTIFIER,s);
	} else {
		s = Find_str_value(&job->destination,IDENTIFIER,Value_sep);
		cccc = Find_flag_value(&job->destination,COPIES,Value_sep);
		n = Find_flag_value(&job->destination,COPY_DONE,Value_sep);
		if( cccc > 1 ){
			SNPRINTF(buffer,sizeof(buffer))"C%d",n+1);
			s = safestrdup2(s,buffer,__FILE__,__LINE__);
			Set_str_value(&controlfile,IDENTIFIER,s);
			if(s) free(s); s = 0;
		} else {
			Set_str_value(&controlfile,IDENTIFIER,s);
		}
	}
	if( !Find_str_value(&controlfile,DATE,Value_sep) ){
		Set_str_value(&controlfile,DATE, Time_str( 0, 0 ) );
	}
	if( (Use_queuename_DYN || Force_queuename_DYN) &&
		!Find_str_value(&controlfile,QUEUENAME,Value_sep) ){
		s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_str_value(&controlfile,QUEUENAME, s );
	}

	/* fix up the control file lines overrided by routing */
	buffer[1] = 0;
	for( i = 0; i < job->destination.count; ++i ){
		s = job->destination.list[i];
		cccc = cval(s);
		buffer[0] = cccc;
		if( isupper(cccc) && Find_str_value(&controlfile,buffer,Value_sep) ){
			Set_str_value( &controlfile,buffer,s+1);
		}
	}

	order = Control_file_line_order_DYN;
    if( !order && Backwards_compatible_DYN ){
        order = BSD_order;
	} else if( !order ){
		order = LPRng_order;
	}
	wildcard = (safestrchr( order,'*') != 0);

	/*
	 * remove any line not required and fix up line metacharacters
	 */

	buffer[1] = 0;
	for( i = 0; i < controlfile.count; ){
		/* get line and first character on line */
		s = controlfile.list[i];
		cccc = *s;
		buffer[0] = cccc;
		/* remove any non-listed options */
		if( (!isupper(cccc) && !isdigit(cccc)) || (!safestrchr(order, cccc) && !wildcard) ){
			Set_str_value( &controlfile,buffer,0);
		} else {
			if( Backwards_compatible_DYN ){
				for( j = 0; maxclen[j].c && cccc != maxclen[j].c ; ++j );
				if( (len = maxclen[j].len) && safestrlen(s+1) > len ){
					s[len+1] = 0;
				}
			}
			++i;
		}
	}

	/*
	 * we check to see if order is correct - we need to check to
	 * see if allowed options in file first.
	 */

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3)Dump_line_list( "Fix_control: before sorting", &controlfile );
#endif
	n = Mergesort( controlfile.list,
		controlfile.count, sizeof( char *), ordercomp, order );
	if( n ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "Fix_control: Mergesort failed" );
	}

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_job( "Fix_control: after sorting", job );
#endif
	for( i = 0; i < controlfile.count; ++i ){
		s = controlfile.list[i];
		memmove(s+1,s+2,safestrlen(s+2)+1);
	}
	s = 0;

	{
		char *datalines;
		char *temp = Join_line_list(&controlfile,"\n");
		DEBUG3( "Fix_control: control info '%s'", temp );

		datalines = Fix_datafile_info( job, number, file_hostname, xlate_format );
		DEBUG3( "Fix_control: data info '%s'", datalines );
		temp = safeextend2(temp,datalines,__FILE__,__LINE__);
		if( datalines ) free(datalines); datalines = 0;
		Set_str_value(&job->info,CF_OUT_IMAGE,temp);
		if( temp ) free(temp); temp = 0;
	}
	
	if( filter ){
		DEBUG3("Fix_control: filter '%s'", filter );

		tempfd = Make_temp_fd( 0 );
		tempfc = Make_temp_fd( 0 );
		s = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep );
		if( Write_fd_str( tempfc, s ) < 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Fix_control: write to tempfile failed" );
		}
		if( lseek( tempfc, 0, SEEK_SET ) == -1 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Fix_control: lseek failed" );
		}
		if( (n = Filter_file( tempfc, tempfd, "CONTROL_FILTER",
			filter, Filter_options_DYN, job, 0, 1 )) ){
			Errorcode = n;
			LOGERR_DIE(LOG_ERR) "Fix_control: control filter failed with status '%s'",
				Server_status(n) );
		}
		s = 0;
		if( n < 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Fix_control: read from tempfd failed" );
		}
		s = Get_fd_image( tempfd, 0 );
		if( s == 0 || *s == 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Fix_control: zero length control filter output" );
		}
		DEBUG4("Fix_control: control filter output '%s'", s);
		Set_str_value(&job->info,CF_OUT_IMAGE,s);
		if(s) free(s); s = 0;
		close( tempfc ); tempfc = -1;
		close( tempfd ); tempfd = -1;
	}
}
#endif

/************************************************************************
 * Create_control:
 *  Create the control file,  setting the various entries.  This is done
 *  on job submission.
 *
 ************************************************************************/

#if defined(JY20031104Create_control)
int Create_control( struct job *job, char *error, int errlen,
	char *xlate_format )
{
	char *fromhost, *file_hostname, *number, *priority, *openname;
	int status = 0, fd, i;
	struct stat statb;

#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL3) Dump_job( "Create_control: before fixing", job );
#endif

	/* deal with authentication */

	Make_identifier( job );

	if( !(fromhost = Find_str_value(&job->info,FROMHOST,Value_sep)) || Is_clean_name(fromhost) ){
		Set_str_value(&job->info,FROMHOST,FQDNRemote_FQDN);
		fromhost = Find_str_value(&job->info,FROMHOST,Value_sep);
	}
	/*
	 * accounting name fixup
	 * change the accounting name ('R' field in control file)
	 * based on hostname
	 *  hostname(,hostname)*=($K|value)*(;hostname(,hostname)*=($K|value)*)*
	 *  we have a semicolon separated list of entrires
	 *  the RemoteHost_IP is compared to these.  If a match is found then the
	 *    user name (if any) is used for the accounting name.
	 *  The user name list has the format:
	 *  ${K}  - value from control file or printcap entry - you must use the
	 *          ${K} format.
	 *  username  - user name value to substitute.
	 */
	if( Accounting_namefixup_DYN ){
		struct line_list l, listv;
		char *accounting_name = 0;
		Init_line_list(&l);
		Init_line_list(&listv);

		DEBUG1("Create_control: Accounting_namefixup '%s'", Accounting_namefixup_DYN );
		Split(&listv,Accounting_namefixup_DYN,";",0,0,0,0,0,0);
		for( i = 0; i < listv.count; ++i ){
			char *s, *t;
			int j;
			s = listv.list[i];
			if( (t = safestrpbrk(s,"=")) ) *t++ = 0;
			Free_line_list(&l);
			DEBUG1("Create_control: hostlist '%s'", s );
			Split(&l,s,",",0,0,0,0,0,0);
			if( Match_ipaddr_value(&l,&RemoteHost_IP) ){
				Free_line_list(&l);
				DEBUG1("Create_control: match, using users '%s'", t );
				Split(&l,t,",",0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
				if(DEBUGL1)Dump_line_list("Create_control: before Fix_dollars", &l );
#endif
				Fix_dollars(&l,job,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
				if(DEBUGL1)Dump_line_list("Create_control: after Fix_dollars", &l );
#endif
				for( j = 0; j < l.count; ++j ){
					if( !ISNULL(l.list[j]) ){
						accounting_name = l.list[j];
						break;
					}
				}
				break;
			}
		}
		DEBUG1("Create_control: accounting_name '%s'", accounting_name );
		if( !ISNULL(accounting_name) ){
			Set_str_value(&job->info,ACCNTNAME,accounting_name );
		}
		Free_line_list(&l);
		Free_line_list(&listv);
	}

	if( Force_IPADDR_hostname_DYN ){
		char buffer[SMALLBUFFER];
		const char *const_s;
		int family;
		/* We will need to create a dummy record. - no host */
		family = RemoteHost_IP.h_addrtype;
		const_s = (char *)inet_ntop( family, RemoteHost_IP.h_addr_list.list[0],
			buffer, sizeof(buffer) );
		DEBUG1("Create_control: remotehost '%s'", const_s );
		Set_str_value(&job->info,FROMHOST,const_s);
		fromhost = Find_str_value(&job->info,FROMHOST,Value_sep);
	}
	{
		char *s, *t;
		if( Force_FQDN_hostname_DYN && !safestrchr(fromhost,'.')
			&& (t = safestrchr(FQDNRemote_FQDN,'.')) ){
			s = safestrdup2(fromhost, t, __FILE__,__LINE__ );
			Set_str_value(&job->info,FROMHOST,s);
			if( s ) free(s); s = 0;
			fromhost = Find_str_value(&job->info,FROMHOST,Value_sep);
		}
	}


	if( !Find_str_value(&job->info,DATE,Value_sep) ){
		char *s = Time_str(0,0);
		Set_str_value(&job->info,DATE,s);
	}
	if( (Use_queuename_DYN || Force_queuename_DYN)
		&& !Find_str_value(&job->info,QUEUENAME,Value_sep) ){
		char *s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_str_value(&job->info,QUEUENAME,s);
		Set_DYN(&Queue_name_DYN,s);
	}
	if( Hld_all(&Spool_control) || Auto_hold_DYN ){
		Set_flag_value( &job->info,HOLD_TIME,time((void *)0) );
	} else {
		Set_flag_value( &job->info,HOLD_TIME,0);
	}

	number = Find_str_value( &job->info,NUMBER,Value_sep);

	priority = Find_str_value( &job->info,PRIORITY,Value_sep);
	if( !priority ){
		priority = Default_priority_DYN;
		if( !priority ) priority = "A";
		Set_str_value(&job->info,PRIORITY,priority);
		priority = Find_str_value(&job->info,PRIORITY,Value_sep);
	}

	file_hostname = Find_str_value(&job->info,FROMHOST,Value_sep);
	if( ISNULL(file_hostname) ) file_hostname = FQDNRemote_FQDN;
	if( ISNULL(file_hostname) ) file_hostname = FQDNHost_FQDN;

	if( isdigit(cval(file_hostname)) ){
		char * s = safestrdup2("ADDR",file_hostname,__FILE__,__LINE__);
		Set_str_value(&job->info,FILE_HOSTNAME,s);
		if( s ) free(s); s = 0;
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
	}
	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);

	/* fix Z options */
	Fix_Z_opts( job );
	/* fix control file name */

	{
		char *s = safestrdup4("cf",priority,number,file_hostname,__FILE__,__LINE__);
		Set_str_value(&job->info,TRANSFERNAME,s);
		if(s) free(s); s = 0;
	}

	{
		/* now we generate the control file image by getting the info
		 * from the hold file
		 */
		char *cf, *datalines;
		cf = datalines = 0;
		for( i = 0; i < job->info.count; ++i ){
			char *t = job->info.list[i];
			int c;
			if( t && (c = t[0]) && isupper(c) && c != 'N' && c != 'U' 
				&& t[1] == '=' ){
				t[1] = 0;
				cf = safeextend4(cf,t,t+2,"\n",__FILE__,__LINE__);
				t[1] = '=';
			}
		}

		DEBUG4("Create_control: first part '%s'", cf );
		datalines = Fix_datafile_info( job, number, file_hostname, xlate_format );
		DEBUG4("Create_control: data info '%s'", datalines );
		cf = safeextend2(cf,datalines,__FILE__,__LINE__);
		DEBUG4("Create_control: joined '%s'", cf );
		if( datalines ) free(datalines); datalines = 0;

		openname = Find_str_value(&job->info,OPENNAME,Value_sep); 
		if( !openname ) openname = Find_str_value(&job->info,TRANSFERNAME,Value_sep); 
		DEBUG4("Create_control: writing to '%s'", openname );
		if( (fd = Checkwrite(openname,&statb,0,1,0)) < 0
			|| ftruncate(fd,0) || Write_fd_str(fd,cf) < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
			SNPRINTF(error,errlen)"Write_control: cannot write '%s' - '%s'",
				openname, Errormsg(errno) );
#endif
			status = 1;
		}
		Max_open(fd);
		if( fd > 0 ) close(fd); fd = -1;
		if( cf ) free(cf); cf = 0;
	}


	return( status );
}
#endif

/*
 * Buffer management
 *  Set up and put values into an output buffer for
 *  transmission at a later time
 */
void Init_buf(char **buf, int *max, int *len)
{
	DEBUG4("Init_buf: buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len );
	if( *max <= 0 ) *max = LARGEBUFFER;
	if( *buf == 0 ) *buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
	*len = 0;
	(*buf)[0] = 0;
}

void Put_buf_len( const char *s, int cnt, char **buf, int *max, int *len )
{
	DEBUG4("Put_buf_len: starting- buf 0x%lx, max %d, len %d, adding %d",
		Cast_ptr_to_long(*buf), *max, *len, cnt );
	if( s == 0 || cnt <= 0 ) return;
	if( *max - *len <= cnt ){
		*max += ((LARGEBUFFER + cnt )/1024)*1024;
		*buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
		DEBUG4("Put_buf_len: update- buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len);
		if( !*buf ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO)"Put_buf_len: realloc %d failed", *len );
		}
	}
	memcpy( *buf+*len, s, cnt );
	*len += cnt;
	(*buf)[*len] = 0;
}

void Put_buf_str( const char *s, char **buf, int *max, int *len )
{
	if( s && *s ) Put_buf_len( s, safestrlen(s), buf, max, len );
}

void Free_buf(char **buf, int *max, int *len)
{
	if( *buf ) free(*buf); *buf = 0;
	*len = 0;
	*max = 0;
}

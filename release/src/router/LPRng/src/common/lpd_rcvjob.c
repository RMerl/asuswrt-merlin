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
"$Id: lpd_rcvjob.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"

#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "getqueue.h"
#include "linksupport.h"
#include "lockfile.h"
#include "permission.h"
#include "proctitle.h"

#include "lpd_remove.h"
#include "lpd_rcvjob.h"
#include "lpd_jobs.h"
#ifdef JYDEBUG//JYWeng
FILE *aaaaaa;
#endif
/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Mon Apr 17 05:43:48 PDT 1995
 * 
 * The protocol used to send a job to a remote host consists of the
 * following:
 * 
 * Client                                   Server
 * \2printername\n - receive a job
 *                                          \0  (ack)
 * \2count controlfilename\n
 * <count bytes>
 * \0
 *                                          \0
 * \3count datafilename\n
 * <count bytes>
 * \0
 *                                          \0
 * \3count datafilename\n
 * <count bytes>
 * \0
 *                                          \0
 * <close connection>
 * 
 * 1. Read the control file from the other end.
 * 2. Check to see if the printer exists,  and if has a printcap entry
 * 3. If it does, check the permissions for the user.
 * 4. Read the job to the queue.
 * 
 * Control file processing
 * 1. The control file at this end might exist already,  and be in use.
 * 	If this is the case,  we will try and allocate another control
 * 	file name if the option is allowed.
 * 2. After we lock the control file,  we will then try and read the
 * 	data files.  Again,  there might be a collision.  If this is
 * 	the case,  then we will again try to generate a new number.
 * 
 * The control file is first read into a file and then read into memory,
 * where it is parsed.  
 * 
 * Duplicate Control/Data files:
 * When copying jobs over,  you might get to a point where you
 * discover that a control and/or data file already exists.
 * 
 * if file already exists:
 * 1. if the existing file length is 0, then you can clobber the file.
 *    This is reasonable given file locking is working
 *    and games are not being played with NFS file systems.
 *    Most likely you have found an abandonded file.
 * 2. If you have the control file and it is locked,
 *    then you might as well clobber the data files
 *    as they are probably left over from another job.
 *    If you do not have the control file,  then you give up
 * 3. If the first file file is the control file,
 *    and you cannot lock it or it is locked and has a non-zero length,
 *    then you should rename the file and try again.
 *    rename the data files/control files
 *    This can be done if the first file is a control file
 *    and you cannot lock it,  or you lock it and it is
 *   non-zero in length.
 * 
 * Job Size:
 *    when the total received job size exceeds limits, then abort job
 *    when the available file space falls below limit, then abort job
 * 
 ***************************************************************************/

int Receive_job( int *sock, char *input )
{
	char line[SMALLBUFFER];		/* line buffer for input */
	char error[SMALLBUFFER];	/* line buffer for input */
	char buffer[SMALLBUFFER];	/* line buffer for input */
	int errlen = sizeof(error);
	char *tempfile;				/* name of temp file */
	double file_len;			/* length of file */
	double read_len;			/* amount to read from sock */
	double jobsize = 0;			/* size of job */
	int ack = 0;				/* ack to send */
	int status = 0;				/* status of the last command */
	double len;					/* length of last read */
	char *s, *filename;			/* name of control or data file */
	int temp_fd = -1;				/* used for file opening and locking */
	int filetype;				/* type of file - control or data */
	int fd;						/* for log file */
	int hold_fd = -1;				/* hold file */
	int db, dbf, rlen;
	int fifo_fd = -1;			/* fifo lock file */
	struct line_list files, info, l;
	struct job job;
	struct stat statb;

#ifdef REMOVE
#ifdef WINDOW_1//JYWeng
//aaaaaa=fopen("/tmp/pp", "a");
//fprintf(aaaaaa, "lpd_rcvjobs.c\n");
//fclose(aaaaaa);
#endif
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Receive_job: check point 1\n");
fclose(aaaaaa);
#endif
#endif
	Init_line_list(&l);
	Init_line_list(&files);
	Init_line_list(&info);
	Init_job(&job);

	Name = "RECV";

	if( input && *input ) ++input;
	Clean_meta(input);
	Split(&info,input,Whitespace,0,0,0,0,0,0);

//printf("Receive_job!\n");//JY1107
/*JY1113: test QueueName*/
	if(get_queue_name(input))
	{
		//printf("QueueName is not LPRServer\n");
		send_ack_packet(sock, ACK_FAIL);//JY1120
		return(0);
	}
	//else printf("QueueName is LPRServer\n");
/**/

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DRECV1)Dump_line_list("Receive_job: input", &info );
#endif
	if( info.count != 1 ){
		SNPRINTF( error, errlen) _("bad command line") );
		goto error;
	}
	if( Is_clean_name( info.list[0] ) ){
		SNPRINTF( error, errlen) _("bad printer name") );
		goto error;
	}

#ifdef REMOVE
	setproctitle( "lpd RECV '%s'", info.list[0] );

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Receive_job: check point 2\n");
fclose(aaaaaa);
#endif
	if( Setup_printer( info.list[0], error, errlen, 0 ) ){
		if( error[0] == 0 ){
			SNPRINTF( error, errlen) _("%s: cannot set up print queue"), Printer_DYN );
		}
		goto error;
	}


	db  = Debug;
	dbf = DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0 );

	if( !(DRECVMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbf;
	} else {
		int i, j;
		i = Debug;
		j = DbgFlag;
		Debug = db;
		DbgFlag = dbf;
		if( Log_file_DYN ){
			fd = Checkwrite( Log_file_DYN, &statb,0,0,0);
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = i;
		DbgFlag = j;
	}

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Receive_job: check point 3\n");
fclose(aaaaaa);
#endif
	DEBUGF(DRECV1)("Receive_job: spooling_disabled %d",
		Sp_disabled(&Spool_control) );
	if( Sp_disabled(&Spool_control) ){
		SNPRINTF( error, errlen)
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}
#endif

	/* send an ACK */
	DEBUGF(DRECV1)("Receive_job: sending 0 ACK for job transfer request" );
	printf("Send ACK\n");

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Receive_job: check point 4\n");
fclose(aaaaaa);
#endif
	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );

	if( status )
	{
		
		SNPRINTF( error, errlen)
			_("%s: Receive_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

#ifdef REMOVE
	/* fifo order enforcement */
	if( Fifo_DYN ){
		char * path = Make_pathname( Spool_dir_DYN, Fifo_lock_file_DYN );
		path = safestrappend4( path,"." , RemoteHost_IP.fqdn, 0  );
		DEBUG1( "Receive_job: checking fifo_lock file '%s'", path );
		fifo_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
		if( fifo_fd < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("Receive_job: cannot open lockfile '%s'"),
				path ); 
		}
		if( Do_lock( fifo_fd, 1 ) < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("Receive_job: cannot lock lockfile '%s'"),
				path ); 
		}
		if(path) free(path); path = 0;
	}

#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "Receive_job: check point 5\n");
fclose(aaaaaa);
#endif
#endif

	while( status == 0 ){
		DEBUGF(DRECV1)("Receive_job: from %s- getting file transfer line", FQDNRemote_FQDN );
		rlen = sizeof(line)-1;
		line[0] = 0;
		status = Link_line_read( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, line, &rlen );
#ifdef JYDEBUG//JYWeng
aaaaaa=fopen("/tmp/pp", "a");
fprintf(aaaaaa, "Receive_job: line=%0x\n", &line[0]);
fclose(aaaaaa);
#endif

		DEBUGF(DRECV1)( "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote_FQDN, status, rlen, line );
#if 0
		LOGMSG(LOG_INFO) "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote_FQDN, status, rlen, line );
#endif


		if( rlen == 0 || status ){
			DEBUGF(DRECV1)( "Receive_job: ending reading from remote" );
			/* treat like normal closing of connection */
			line[0] = 0;
			status = 0;
			break;
		}

		filetype = line[0];
		Clean_meta(line+1);

		/* make sure we have a data file transfer */
		if( filetype != DATA_FILE && filetype != CONTROL_FILE ){
			/* we may have another type of command */
			status = 0;
			break;
		}
		/* make sure we have length and filename */

		filename = 0;
		file_len = strtod(line+1,&filename);
		if ((line+1) == filename){
			/* Recover from Apple Desktop Printing stupidity.
			   It occasionally resends the queue selection cmd.
			   Darian Davis DD 03JUL2000 */
			status = 0;
			LOGERR(LOG_ERR)"Recovering from incorrect job submission");
			continue;
		}

		if( filename ){
			while( isspace(cval(filename)) ) ++filename;
			Clean_meta(filename);
			s = filename;
			while( (s = strpbrk(s," \t")) ) *s++ = '_';
		}
		if( file_len < 0
			|| filename == 0 || *filename == 0
			|| (file_len == 0 && filetype != DATA_FILE) ){
			ack = ACK_STOP_Q;
			SNPRINTF( error, errlen)
			_("%s: Receive_job - bad control line '%s', len %0.0f, name '%s'"),
				Printer_DYN, line, file_len, filename );
			goto error;
		}


		/************************************************
		 * check for job size and available space
		 * This is done here so that we can neatly clean up
		 * if we need to. Note we do this after we truncate...
		 ************************************************/
		jobsize += file_len;
		read_len = file_len;


		if( read_len == 0 ) read_len = Max_job_size_DYN*1024;
		if( Max_job_size_DYN > 0 && (jobsize/1024) > (0.0+Max_job_size_DYN) ){
			SNPRINTF( error, errlen)
				_("%s: job size %0.3fK is larger than %d K"),
				Printer_DYN, jobsize/1024, Max_job_size_DYN );
			ack = ACK_RETRY;
			goto error;
		} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
			SNPRINTF( error, errlen)
				_("%s: insufficient file space"), Printer_DYN );
			ack = ACK_RETRY;
			goto error;
		}


		/*
		 * we are ready to read the file; send 0 ack saying so
		 */

		DEBUGF(DRECV2)("Receive_job: sending 0 ACK to transfer '%s'", filename );
		status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );
		if( status ){
			SNPRINTF( error, errlen)
				_("%s: sending ACK 0 for '%s' failed"), Printer_DYN, filename );
			ack = ACK_RETRY;
			goto error;
		}


		temp_fd = Make_temp_fd(&tempfile);

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */

		DEBUGF(DRECV4)("Receive_job: receiving '%s' %d bytes ", filename, read_len );
		len = read_len;
#if TEST_WRITE//JYWeng
	if(filetype != DATA_FILE){
#if 1//JY1110
		status = Link_file_read( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
#else
		status = Link_file_read_test( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
#endif
		}
	else	{
		status = Link_file_read_test( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
		}
#else
		status = Link_file_read( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
#endif

		DEBUGF(DRECV4)("Receive_job: status %d, read_len %0.0f, file_len %0.0f",
			status, read_len, file_len );

		/* close the file */
		close(temp_fd);
		temp_fd = -1;

		if( status 
			|| (file_len == 0 && read_len == 0)
			|| (file_len != 0 && file_len != read_len) )
		{
			printf("Why error %lf %lf %d\n", file_len, read_len, status);
			SNPRINTF( error, errlen)
				_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
				filename, ShortRemote_FQDN );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * we process the control file and make sure we can print it
		 */
		printf(" Control file\n");

#if defined(JYWENG20031104CONTROL)
		if( filetype == CONTROL_FILE ){
			DEBUGF(DRECV2)("Receive_job: receiving new control file, old job.info.count %d, old files.count %d",
				job.info.count, files.count );
			if( job.info.count ){
				/* we received another control file */
				if( Check_for_missing_files(&job, &files, error, errlen, 0, &hold_fd) ){
					goto error;
				}
				hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
			}
			Free_job(&job);
			Set_str_value(&job.info,OPENNAME,tempfile);
			Set_str_value(&job.info,TRANSFERNAME,filename);
			hold_fd = Set_up_temporary_hold_file( &job, error, errlen );
			if( files.count ){
				/* we have datafiles, FOLLOWED by a control file,
					followed (possibly) by another control file */
				/* we receive another control file */
				if( Check_for_missing_files(&job, &files, error, errlen, 0, &hold_fd) ){
					goto error;
				}
				hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
				Free_job(&job);
			}
		} else {
			Set_casekey_str_value(&files,filename,tempfile);
		}
#endif
		DEBUGF(DRECV2)("Receive_job: sending 0 ACK transfer done" );
		status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	}

	DEBUGF(DRECV2)("Receive_job: eof on transfer, job.info.count %d, files.count %d",
		job.info.count, files.count );

#if defined(JYWENG20031104Check_for_missing_files)
	if( job.info.count ){
		/* we receive another control file */
		if( Check_for_missing_files(&job, &files, error, errlen, 0, &hold_fd) ){
			goto error;
		}
		hold_fd = -1;
		Free_line_list(&files);
		jobsize = 0;
		Free_job(&job);
	}
#endif

/*JY1110*/


 error:

#if 0//JY1111
	if( temp_fd > 0 ) close(temp_fd); temp_fd = -1;
	if( fifo_fd > 0 ){
		Do_unlock( fifo_fd );
		close(fifo_fd); fifo_fd = -1;
	}

	Remove_tempfiles();
	if( error[0] ){
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGF(DRECV1)("Receive_job: error, removing job" );
		DEBUGFC(DRECV4)Dump_job("Receive_job - error", &job );
#endif
		s = Find_str_value(&job.info,HF_NAME,Value_sep);
		if( !ISNULL(s) ) unlink(s);
		if( ack == 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		SNPRINTF(buffer+1,sizeof(buffer)-1)"%s\n",error);
		/* LOG( LOG_INFO) "Receive_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, safestrlen(buffer), 0 );
		Link_close( sock );
		if( hold_fd >= 0 ){
			close(hold_fd); hold_fd = -1;
		}
	} else {
		Link_close( sock );
		/* update the spool queue */
		Get_spool_control( Queue_control_file_DYN, &Spool_control );
		Set_flag_value(&Spool_control,CHANGE,1);
		Set_spool_control( 0, Queue_control_file_DYN, &Spool_control );
		if( Lpq_status_file_DYN ){
			unlink( Lpq_status_file_DYN );
		}
		s = Server_queue_name_DYN;
		if( !s ) s = Printer_DYN;

		SNPRINTF( line, sizeof(line)) "%s\n", s );
		DEBUGF(DRECV1)("Receive_jobs: Lpd_request fd %d, starting '%s'", Lpd_request, line );
		if( Write_fd_str( Lpd_request, line ) < 0 ){
			LOGERR_DIE(LOG_ERR) _("Receive_jobs: write to fd '%d' failed"),
				Lpd_request );
		}
		Free_line_list(&info);
		Free_line_list(&files);
		Free_job(&job);
		Free_line_list(&l);

		/* Do_queue_jobs( s, 0 ); */
	}
	Free_line_list(&info);
	Free_line_list(&files);
	Free_job(&job);
	Free_line_list(&l);

	cleanup( 0 );
#endif//JY1111

/*JY1111*/
	check_prn_status(ONLINE, "");
	return(0);
}

#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Block Job Transfer
 * \RCV_BLOCKprinter size
 *   The actual file transferred has the format:
 *   \CONTROL_FILElen name
 *   [control file contents]
 *   \DATA_FILElen name
 *   [data file contents]
 *
 * We receive the entire file, placing it into the control file.
 * We then split the job up as usual
 ***************************************************************************/

#define MAX_INPUT_TOKENS 10

int Receive_block_job( int *sock, char *input )
{
	int temp_fd = -1, fd;	/* fd for received file */
	double read_len;	/* file read length */
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	char buffer[SMALLBUFFER];
	int ack = 0, status = 0;
	double file_len;
	char *tempfile, *s;
	struct stat statb;
	struct line_list l;
	int db, dbf;


	error[0] = 0;
	Init_line_list(&l);

	Name = "RECVB";

	if( *input ) ++input;
	Clean_meta(input);
	Split(&l,input,Whitespace,0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DRECV1)Dump_line_list("Receive_block_job: input", &l );
#endif

	if( l.count != 2 ){
		SNPRINTF( error, errlen-4) _("bad command line") );
		goto error;
	}
	if( Is_clean_name( l.list[0] ) ){
		SNPRINTF( error, errlen-4) _("bad printer name") );
		goto error;
	}
	setproctitle( "lpd RECVB '%s'", l.list[0] );

	if( Setup_printer( l.list[0], error, errlen-4, 0 ) ){
		if( error[0] == 0 ){
			SNPRINTF( error, errlen-4) _("%s: cannot set up printer"), Printer_DYN );
		}
		goto error;
	}


	db = Debug;
	dbf =DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0 );

	if( !(DRECVMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbf;
	} else {
		dbf = Debug;
		Debug = db;
		if( Log_file_DYN ){
			fd = Checkwrite( Log_file_DYN, &statb,0,0,0);
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = dbf;
	}

#ifndef NODEBUG 
	DEBUGF(DRECV1)("Receive_block_job: debug '%s', Debug %d, DbgFlag 0x%x", s, Debug, DbgFlag );
#endif


	DEBUGF(DRECV1)("Receive_block_job: spooling_disabled %d", Sp_disabled(&Spool_control) );
	if( Sp_disabled(&Spool_control) ){
		SNPRINTF( error, errlen-4)
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* check for space */

	file_len  = strtod( l.list[1], 0 );
	read_len = file_len;

	if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
		SNPRINTF( error, errlen)
			_("%s: job size %0.3f is larger than %dK"),
			Printer_DYN, file_len/1024, Max_job_size_DYN );
		ack = ACK_RETRY;
		goto error;
	} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
		SNPRINTF( error, errlen-4)
			_("%s: insufficient file space"), Printer_DYN );
		ack = ACK_RETRY;
		goto error;
	}

	/*
	 * we are ready to read the file; send 0 ack saying so
	 */

	DEBUGF(DRECV1)("Receive_block_job: sending 0 ACK for job transfer request" );

	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: Receive_block_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

	temp_fd = Make_temp_fd( &tempfile );
	DEBUGF(DRECV4)("Receive_block_job: receiving '%s' %0.0f bytes ", tempfile, file_len );
	status = Link_file_read( ShortRemote_FQDN, sock,
		Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
	DEBUGF(DRECV4)("Receive_block_job: received %d bytes ", read_len );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
			tempfile, ShortRemote_FQDN );
		ack = ACK_FAIL;
		goto error;
	}

	/* extract jobs */

	if( lseek( temp_fd, 0, SEEK_SET ) == -1 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error, errlen-4)	
			_("Receive_block_job: lseek failed '%s'"), Errormsg(errno) );
#endif
		ack = ACK_FAIL;
		goto error;
	}

	if( Scan_block_file( temp_fd, error, errlen-4, 0 ) ){
		ack = ACK_FAIL;
		goto error;
	}

	close( temp_fd );
	temp_fd = -1;

	DEBUGF(DRECV2)("Receive_block_job: sending 0 ACK" );
	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: sending ACK 0 for '%s' failed"), Printer_DYN, tempfile );
		ack = ACK_RETRY;
		goto error;
	}
	error[0] = 0;

 error:
	Free_line_list(&l);
	if( temp_fd > 0 ){
		close(temp_fd );
	}
	if( error[0] ){
		if( ack != 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		SNPRINTF(buffer+1,sizeof(buffer)-1)"%s\n",error);
		/* LOG( LOG_INFO) "Receive_block_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_block_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, safestrlen(buffer), 0 );
		Link_close( sock );
	} else {
		Link_close( sock );
		Remove_tempfiles();

		s = Server_queue_name_DYN;
		if( !s ) s = Printer_DYN;

		SNPRINTF( buffer, sizeof(buffer)) "%s\n", s );
		DEBUGF(DRECV1)("Receive_block_jobs: Lpd_request fd %d, starting '%s'", Lpd_request, buffer );
		if( Write_fd_str( Lpd_request, buffer ) < 0 ){
			LOGERR_DIE(LOG_ERR) _("Receive_block_jobs: write to fd '%d' failed"),
				Lpd_request );
		}
	}
	return( error[0] != 0 );
}
#endif


#ifdef ORIGINAL_DEBUG//JY@1020
/***************************************************************************
 * Scan_block_file( int fd, struct control_file *cfp )
 *  we scan the block file, getting the various portions
 *
 * Generate the compressed data files - this has the format
 *    \3count cfname\n
 *    [count control file bytes]
 *    \4count dfname\n
 *    [count data file bytes]
 *
 *  We extract the various sections and find the offsets.
 *  Note that the various name fields will be the original
 *  values;  the ones we actually use will be the transfer values
 * RETURNS: nonzero on error, error set
 *          0 on success
 ***************************************************************************/

int Scan_block_file( int fd, char *error, int errlen, struct line_list *header_info )
{
	char line[LINEBUFFER];
	char buffer[LARGEBUFFER];
	int startpos;
	int read_len, filetype, tempfd = -1;		/* type and length fields */
	char *filename;				/* name field */
	char *tempfile;				/* name field */
	int status;
	int len, count, n;
	int hold_fd = -1;
	struct line_list l, info, files;
	struct job job;
	struct stat statb;

	if( fstat( fd, &statb) < 0 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO)"Scan_block_file: fstat failed");
	}
	DEBUGF(DRECV2)("Scan_block_file: starting, file size '%0.0f'",
		(double)(statb.st_size) );
	Init_line_list(&l);
	Init_line_list(&info);
	Init_line_list(&files);
	Init_job(&job);

	/* first we find the file position */
	
	startpos = lseek( fd, 0, SEEK_CUR );
	DEBUGF(DRECV2)("Scan_block_file: starting at %d", startpos );
	while( (status = Read_one_line( fd, line, sizeof(line) )) > 0 ){
		/* the next position is the start of data */
		Free_line_list(&l);
		Free_line_list(&info);
		startpos = lseek( fd, 0, SEEK_CUR );
		if( startpos == -1 ){
#ifdef ORIGINAL_DEBUG//JY@1020
			SNPRINTF( error, errlen)	
				_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
#endif
			status = 1;
			goto error;
		}
		DEBUGF(DRECV2)("Scan_block_file: '%s', end position %d",
			line, startpos );
		filetype = line[0];
		if( filetype != CONTROL_FILE && filetype != DATA_FILE ){
			/* get the next line */
			continue;
		}
		Clean_meta(line+1);
		Split(&info,line+1,Whitespace,0,0,0,0,0,0);
		if( info.count != 2 ){
			SNPRINTF( error, errlen)
			_("bad length information '%s'"), line+1 );
			status = 1;
			goto error;
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DRECV2)Dump_line_list("Scan_block_file- input", &info );
#endif
		read_len = atoi( info.list[0] );
		filename = info.list[1];
		tempfd = Make_temp_fd( &tempfile );
		DEBUGF(DRECV2)("Scan_block_file: tempfd %d, read_len %d", read_len, tempfd );
		for( len = read_len; len > 0; len -= count ){
			n = sizeof(buffer);
			if( n > len ) n = len;
			count = read(fd,buffer,n);
			DEBUGF(DRECV2)("Scan_block_file: len %d, reading %d, got count %d",
				len, n, count );
			if( count < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
				SNPRINTF( error, errlen)	
					_("Scan_block_file: read failed '%s'"), Errormsg(errno) );
#endif
				status = 1;
				goto error;
			} else if( count == 0 ){
				SNPRINTF( error, errlen)	
					_("Scan_block_file: read unexecpted EOF") );
				status = 1;
				goto error;
			}
			n = write(tempfd,buffer,count);
			if( n != count ){
#ifdef ORIGINAL_DEBUG//JY@1020
				SNPRINTF( error, errlen)	
					_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
#endif
				status = 1;
				goto error;
			}
		}
		close( tempfd);
		tempfd = -1;

		if( filetype == CONTROL_FILE ){
			DEBUGF(DRECV2)("Scan_block_file: control file '%s'", filename );
			DEBUGF(DRECV2)("Scan_block_file: received control file, job.info.count %d, files.count %d",
				job.info.count, files.count );

#if defined(JYWENG20031104Config_value_conversion)
			if( job.info.count ){
				if( Check_for_missing_files(&job, &files, error, errlen, 0, &hold_fd) ){
					goto error;
				}
				hold_fd = -1;
				Free_line_list(&files);
				Free_job(&job);
			}
#endif
			Set_str_value(&job.info,OPENNAME,tempfile);
			Set_str_value(&job.info,TRANSFERNAME,filename);
			hold_fd = Set_up_temporary_hold_file( &job, error, errlen );
			if( hold_fd < 0 ) goto error;
#if defined(JYWENG20031104Config_value_conversion)
			if( files.count ){
				/* we have datafiles, FOLLOWED by a control file,
					followed (possibly) by another control file */
				/* we receive another control file */
				if( Check_for_missing_files(&job, &files, error, errlen, 0, &hold_fd) ){
					goto error;
				}
				hold_fd = -1;
				Free_line_list(&files);
				Free_job(&job);
			}
#endif
		} else {
			Set_str_value(&files,filename,tempfile);
		}
	}

#if defined(JYWENG20031104Config_value_conversion)
	if( files.count ){
		/* we receive another control file */
		if( Check_for_missing_files(&job, &files, error, errlen, header_info, &hold_fd) ){
			goto error;
		}
		hold_fd = -1;
		Free_line_list(&files);
		Free_job(&job);
	}
#endif

 error:
	if( hold_fd >= 0 ){
		Remove_job( &job );
		close(hold_fd); hold_fd = -1;
	}
	if( tempfd >= 0 ) close(tempfd); tempfd = -1;
	Free_line_list(&l);
	Free_line_list(&info);
	Free_line_list(&files);
	Free_job(&job);
	return( status );
}
#endif

/***************************************************************************
 * static int read_one_line(int fd, char *buffer, int maxlen );
 *  reads one line (terminated by \n) into the buffer
 *RETURNS:  0 if EOF characters read
 *          n = # chars read
 *          Note: buffer terminated by 0
 ***************************************************************************/
int Read_one_line( int fd, char *buffer, int maxlen )
{
	int len, status;
	len = status = 0;

	while( len < maxlen-1 && (status = read( fd, &buffer[len], 1)) > 0 ){
		if( buffer[len] == '\n' ){
			break;
		}
		++len;
	}
	buffer[len] = 0;
	return( status );
}

int Check_space( double jobsize, int min_space, char *pathname )
{
	double space = Space_avail(pathname);
	int ok;

	jobsize = ((jobsize+1023)/1024);

	ok = ((jobsize + min_space) < space);
#ifdef RETURNOK 
aaaaaa=fopen("/tmp/qqqqq", "a");
fprintf(aaaaaa, "PATH=%s\n", pathname);
fprintf(aaaaaa, "*********************************************************\n");
fprintf(aaaaaa, "min_space=%d\n", min_space);
fprintf(aaaaaa, "jobsize=%f\n", jobsize);
fprintf(aaaaaa, "availspace=%f\n", space);
fprintf(aaaaaa, "ok=%d\n", ok);
fprintf(aaaaaa, "*********************************************************\n");
fclose(aaaaaa);
#endif

	DEBUGF(DRECV1)("Check_space: path '%s', space %0.0f, jobsize %0.0fK, ok %d",
		pathname, space, jobsize, ok );

#ifdef RETURNOK 
	return( ok );//JYWeng
#else
	return( 1 );
#endif
}

#if defined(JYWENG20031104Do_perm_check)
int Do_perm_check( struct job *job, char *error, int errlen )
{
	int permission = 0;			/* permission */
	char *s;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DRECV1)Dump_job("Do_perm_check", job );
#endif
	Perm_check.service = 'R';
	Perm_check.printer = Printer_DYN;
	s = Find_str_value(&job->info,LOGNAME,Value_sep);
	Perm_check.user = s;
	Perm_check.remoteuser = s;
	Perm_check.host = 0;
	s = Find_str_value(&job->info,FROMHOST,Value_sep);
	if( s && Find_fqdn( &PermHost_IP, s ) ){
		Perm_check.host = &PermHost_IP;
	}
	Perm_check.remotehost = &RemoteHost_IP;

	/* check for permission */

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list, "");
	}

	if( (permission = Perms_check( &Perm_line_list, &Perm_check, job, 1 ))
			== P_REJECT ){
		SNPRINTF( error, errlen)
			_("%s: no permission to print"), Printer_DYN );
	}
	Perm_check.user = 0;
	Perm_check.remoteuser = 0;
	DEBUGF(DRECV1)("Do_perm_check: permission '%s'", perm_str(permission) );
	return( permission );
}
#endif

/*
 * Process the list of control and data files, and make a job from them
 *  job - the job structure
 *  files - list of files that we received and need to check.
 *    if this is a copy of a job from another queue, files == 0
 *  spool_control - the spool control values for this queue
 *  spool_dir - the spool directory for this queue
 *  xlate_incoming_format - only valid for received files
 *  error, errlen - the error message information
 *  header_info - authentication ID to put in the job
 *   - if 0, do not update, this preserves copy
 *  returns: 0 - successful
 *          != 0 - error
 */

#if defined(JYWENG20031104Check_for_missing_files)
int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen, struct line_list *header_info, int *holdfile_fd )
{
	int count;
	struct line_list *lp = 0, datafiles;
	int status = 0;
	char *openname, *transfername;
	double jobsize;
	int copies;
	struct stat statb;
	struct timeval start_time;

	if( gettimeofday( &start_time, 0 ) ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Check_for_missing_files: gettimeofday failed");
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUG1("Check_for_missing_files: holdfile_fd %d, start time 0x%x usec 0x%x",
		*holdfile_fd,
		(int)start_time.tv_sec, (int)start_time.tv_usec );
	if(DEBUGL1)Dump_job("Check_for_missing_files - start", job );
	if(DEBUGL1)Dump_line_list("Check_for_missing_files- files", files );
	if(DEBUGL1)Dump_line_list("Check_for_missing_files- header_info", header_info );
#endif

	Set_flag_value(&job->info,JOB_TIME,(int)start_time.tv_sec);
	Set_flag_value(&job->info,JOB_TIME_USEC,(int)start_time.tv_usec);

	Init_line_list(&datafiles);

	/* we can get this as a new job or as a copy.
	 * if we get a copy,  we do not need to check this stuff
	 */
	if( !Find_str_value(&job->info,REMOTEHOST,Value_sep) ){
		Set_str_value(&job->info,REMOTEHOST,RemoteHost_IP.fqdn);
		Set_flag_value(&job->info,UNIXSOCKET,Perm_check.unix_socket);
		Set_flag_value(&job->info,REMOTEPORT,Perm_check.port);
	}
	if( files ){
		if( Do_perm_check( job, error, errlen ) == P_REJECT ){
			status = 1;
			goto error;
		}

		jobsize = 0;
		error[0] = 0;
		/* RedHat Linux 6.1 - sends a control file with NO data files */
		if( job->datafiles.count == 0 && files->count > 0 ){
			Check_max(&job->datafiles,files->count+1);
			for( count = 0; count < files->count; ++count  ){
				lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
				memset(lp,0,sizeof(lp[0]));
				job->datafiles.list[job->datafiles.count++] = (void *)lp;
				/*
				 * now we add the information needed
				 *  the files list has 'transfername=openname'
				 */
				transfername = files->list[count];
				if( (openname = strchr(transfername,'=')) ) *openname++ = 0;
				Set_str_value(lp,TRANSFERNAME,transfername);
				Set_str_value(lp,FORMAT,"f");
				Set_flag_value(lp,COPIES,1);
				if( openname ) openname[-1] = '=';
			}
#ifdef ORIGINAL_DEBUG//JY@1020
			if(DEBUGL1)Dump_job("RedHat Linux fix", job );
#endif
		}
		for( count = 0; count < job->datafiles.count; ++count ){
			lp = (void *)job->datafiles.list[count];
			transfername = Find_str_value(lp,OTRANSFERNAME,Value_sep);
			if( ISNULL(transfername) ) transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
			/* find the open name and replace it in the information */
			if( (openname = Find_casekey_str_value(files,transfername,Value_sep)) ){
				Set_str_value(lp,OPENNAME,openname);
				Set_casekey_str_value(&datafiles,transfername,openname);
			} else {
				SNPRINTF(error,errlen)"missing data file '%s'",transfername);
				status = 1;
				goto error;
			}
			if( (status = stat( openname, &statb )) ){
#ifdef ORIGINAL_DEBUG//JY@1020
					SNPRINTF( error, errlen) "stat() '%s' error - %s",
					openname, Errormsg(errno) );
#endif
				goto error;
			}
			copies = Find_flag_value(lp,COPIES,Value_sep);
			if( copies == 0 ) copies = 1;
			jobsize += copies * statb.st_size;
		}
		Set_double_value(&job->info,SIZE,jobsize);

#ifdef ORIGINAL_DEBUG//JY@1020
		if(DEBUGL1)Dump_line_list("Check_for_missing_files- found", &datafiles );
#endif
		if( files->count != datafiles.count ){
			SNPRINTF(error,errlen)"too many data files");
			status = 1;
			goto error;
		}
		Free_line_list(&datafiles);
	}

	/* now we need to assign a control file number */
	if( *holdfile_fd <= 0 && (*holdfile_fd = Find_non_colliding_job_number( job )) < 0 ){
		SNPRINTF(error,errlen)
			"cannot allocate hold file");
		status = 1;
		goto error;
	}
	DEBUG1("Check_for_missing_files: holdfile_fd  now '%d'", *holdfile_fd );

	if( header_info && User_is_authuser_DYN ){
		char *s = Find_str_value(header_info,AUTHUSER,Value_sep);
		if( !ISNULL(s) ){
			Set_str_value( &job->info,LOGNAME,s);
			DEBUG1("Check_for_missing_files: setting user to authuser '%s'", s );
		}
	}
#if defined(JYWENG20031104Create_control)
	if( Create_control( job, error, errlen, Xlate_incoming_format_DYN ) ){
		DEBUG1("Check_for_missing_files: Create_control error '%s'", error );
		status = 1;
		goto error;
	}
#endif
	Set_str_value(&job->info,HPFORMAT,0);
	Set_str_value(&job->info,INCOMING_TIME,0);

	if( header_info ){
		char *authfrom, *authuser, *authtype, *authca;
		authfrom = Find_str_value(header_info,AUTHFROM,Value_sep);
		authuser = Find_str_value(header_info,AUTHUSER,Value_sep);
		authtype = Find_str_value(header_info,AUTHTYPE,Value_sep);
		authca = Find_str_value(header_info,AUTHCA,Value_sep);
		if( ISNULL(authuser) ) authuser = authfrom;
		Set_str_value(&job->info,AUTHUSER,authuser);
		Set_str_value(&job->info,AUTHFROM,authfrom);
		Set_str_value(&job->info,AUTHTYPE,authtype);
		Set_str_value(&job->info,AUTHCA,authca);
	}
	/* now we do the renaming */
	status = 0;
	for( count = 0; status == 0 && count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		openname = Find_str_value(lp,OPENNAME,Value_sep);
		if( stat(openname,&statb) ) continue;
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		DEBUG1("Check_for_missing_files: renaming '%s' to '%s'",
			openname, transfername );
		if( (status = rename(openname,transfername)) ){
#ifdef ORIGINAL_DEBUG//JY@1020
			SNPRINTF( error,errlen)
				"error renaming '%s' to '%s' - %s",
				openname, transfername, Errormsg( errno ) );
#endif
		}
	}
	if( status ) goto error;
	if( (status = Get_route( job, error, errlen )) ){
		DEBUG1("Check_for_missing_files: Routing_filter error '%s'", error );
		goto error;
	}
	openname = Find_str_value(&job->info,OPENNAME,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	DEBUG1("Check_for_missing_files: renaming '%s' to '%s'",
		openname, transfername );
	if( (status = rename(openname,transfername)) ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error,errlen)
			"error renaming '%s' to '%s' - %s",
			openname, transfername, Errormsg( errno ) );
#endif
		goto error;
	}
	if( (status = Set_hold_file( job, 0, *holdfile_fd )) ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error,errlen)
			"error setting up hold file - %s",
			Errormsg( errno ) );
#endif
		goto error;
	}
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1)Dump_job("Check_for_missing_files - ending", job );
#endif

 error:
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( status ){
		LOGMSG(LOG_INFO) "Check_for_missing_files: FAIL '%s' %s", transfername, error);
		/* we need to unlink the data files */
		openname = Find_str_value(&job->info,OPENNAME,Value_sep);
		transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		if( openname ) unlink( openname );
		if( transfername) unlink( transfername );
		for( count = 0; count < job->datafiles.count; ++count ){
			lp = (void *)job->datafiles.list[count];
			transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
			openname = Find_str_value(lp,OPENNAME,Value_sep);
			unlink(openname);
			unlink(transfername);
		}
		openname = Find_str_value(&job->info,HF_NAME,Value_sep);
		if( openname ) unlink(openname);
	} else {
		/*
		LOGMSG(LOG_INFO) "Check_for_missing_files: SUCCESS '%s'", transfername);
		*/
#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( job, "STATE", "CREATE" );
#endif
	}

	if( *holdfile_fd >= 0 ) close(*holdfile_fd); *holdfile_fd = -1;
	Free_line_list(&datafiles);
	return( status );
}
#endif

#if defined(JYWENG200301104Set_up_temporary_hold_file)
/***************************************************************************
 * int Set_up_temporary_hold_file( struct job *job,
 *	char *error, int errlen )
 *  sets up a hold file and control file
 ***************************************************************************/

int Set_up_temporary_hold_file( struct job *job,
	char *error, int errlen  )
{
	int fd = -1;
	/* now we need to assign a control file number */
	DEBUG1("Set_up_temporary_hold_file: starting" );

	/* sets identifier and hold information */
	Setup_job( job, &Spool_control, 0, 0, 0);
	/* now we get collision resolution */
	if( (fd = Find_non_colliding_job_number( job )) < 0 ){
		SNPRINTF(error,errlen)
			"cannot allocate hold file");
		goto error;
	}
	DEBUG1("Set_up_temporary_hold_file: hold file fd '%d'", fd );
	
	/* mark as incoming */
	Set_flag_value(&job->info,INCOMING_TIME,time((void *)0) );
	/* write status */
	if( Set_hold_file( job, 0, fd ) ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error,errlen)
			"error setting up hold file - %s",
			Errormsg( errno ) );
#endif
		close(fd); fd = -1;
		goto error;
	}
 error:
	return( fd );
}
#endif

/***************************************************************************
 * int Find_non_colliding_job_number( struct job *job )
 *  Find a non-colliding job number for the new job
 * RETURNS: 0 if successful
 *          ack value if unsuccessful
 * Side effects: sets up control file fields;
 ***************************************************************************/

int Find_non_colliding_job_number( struct job *job )
{
	int hold_fd = -1;			/* job hold file fd */
	struct stat statb;			/* for status */
	char hold_file[SMALLBUFFER], *number;
	int max, n, start;

	/* we set the job number to a reasonable range */
	hold_fd = -1;
	number = Fix_job_number(job,0);
	start = n = strtol(number,0,10);
	max = 1000;
	if( Long_number_DYN ) max = 1000000;
	while( hold_fd < 0 ){
		number = Fix_job_number(job,n);
		SNPRINTF(hold_file,sizeof(hold_file))"hfA%s",number);
		DEBUGF(DRECV1)("Find_non_colliding_job_number: trying %s", hold_file );
		hold_fd = Checkwrite(hold_file, &statb,
			O_RDWR|O_CREAT, 0, 0 );
		/* if the hold file locked or is non-zero, we skip to a new one */
		if( hold_fd < 0 || Do_lock( hold_fd, 0 ) < 0 || statb.st_size ){
			close( hold_fd );
			hold_fd = -1;
			hold_file[0] = 0;
			++n;
			if( n > max ) n = 0;
			if( n == start ){
				break;
			}
		} else {
			Set_str_value(&job->info,HF_NAME,hold_file);
		}
	}
	DEBUGF(DRECV1)("Find_non_colliding_job_number: using %s", hold_file );
	return( hold_fd );
}

int Get_route( struct job *job, char *error, int errlen )
#ifdef ORIGINAL_DEBUG//JY@1020
{
#else
{}
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	int i, fd, tempfd, count, c;
	char *tempfile, *openname, *s, *t, *id;
	char buffer[SMALLBUFFER];
	int errorcode = 0;
	struct line_list info, dest, env, *lp, cf_line_list;

	DEBUG1("Get_route: routing filter '%s', control filter '%s'",
		Routing_filter_DYN, Incoming_control_filter_DYN );
	if( Routing_filter_DYN == 0 && Incoming_control_filter_DYN == 0 ){
		return(errorcode);
	}

	Init_line_list(&info);
	Init_line_list(&dest);
	Init_line_list(&env);
	Init_line_list(&cf_line_list);

	/* build up the list of files and initialize the DATAFILES
	 * environment variable
	 */

	for( i = 0; i < job->datafiles.count; ++i ){
		lp = (void *)job->datafiles.list[i];
		openname = Find_str_value(lp,TRANSFERNAME,Value_sep);
		Add_line_list(&env,openname,Value_sep,1,1);
	}
	s = Join_line_list_with_sep(&env," ");
	Free_line_list( &env );
	Set_str_value(&env,DATAFILES,s);
	free(s); s = 0;

	openname = Find_str_value(&job->info,OPENNAME,Value_sep);
	if( (fd = open(openname,O_RDONLY,0)) < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF(error,errlen)"Get_route: open '%s' failed '%s'",
			openname, Errormsg(errno) );
#endif
		errorcode = 1;
		goto error;
	}
	Max_open(fd);
	tempfd = Make_temp_fd(&tempfile);

	/* we pass the control file through the incoming control filter */
	if( Incoming_control_filter_DYN ){
		DEBUG1("Get_route: running '%s'",
			Incoming_control_filter_DYN );
		errorcode = Filter_file( fd, tempfd, "INCOMING_CONTROL_FILTER",
			Incoming_control_filter_DYN, Filter_options_DYN, job, &env, 0);
		switch(errorcode){
			case 0: break;
			case JHOLD:
				Set_flag_value(&job->info,HOLD_TIME,time((void *)0) );
				errorcode = 0;
				break;
			case JREMOVE: break;
			default:
				SNPRINTF(error,errlen)"Get_route: incoming control filter '%s' failed '%s'",
					Incoming_control_filter_DYN, Server_status(errorcode) );
				errorcode = JFAIL;
				goto error;
		}
		close(fd); close(tempfd); fd = -1; tempfd = -1;
		if( rename( tempfile, openname ) == -1 ){
#ifdef ORIGINAL_DEBUG//JY@1020
			SNPRINTF(error,errlen)"Get_route: rename '%s' to '%s' failed - %s",
				tempfile, openname, Errormsg(errno) );
#endif
			errorcode = 1;
			goto error;
		}
		if( (fd = open(openname,O_RDONLY,0)) < 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
			SNPRINTF(error,errlen)"Get_route: open '%s' failed '%s'",
				openname, Errormsg(errno) );
#endif
			errorcode = 1;
			goto error;
		}
		Max_open(fd);
		if( Get_file_image_and_split(openname,0,0, &cf_line_list, Line_ends,0,0,0,0,0,0) ){
#ifdef ORIGINAL_DEBUG//JY@1020
            SNPRINTF(error,errlen)
                "Get_route: open failed - modified control file  %s - %s", openname, Errormsg(errno) );
#endif
			goto error;
		}
		for( i = 'A'; i <= 'Z'; ++i ){
			buffer[1] = 0;
			buffer[0] = i;
			Set_str_value(&job->info,buffer,0);
		}
		for( i = 0; i < cf_line_list.count; ++i ){
			s = cf_line_list.list[i];
			Clean_meta(s);
			c = cval(s);
			DEBUG3("Get_route: doing line '%s'", s );
			if( isupper(c) && c != 'U' && c != 'N' ){
				buffer[0] = c; buffer[1] = 0;
				DEBUG3("Get_route: control '%s'='%s'", buffer, s+1 );
				Set_str_value(&job->info,buffer,s+1);
			}
		}
		tempfd = Make_temp_fd(&tempfile);
	}

	if( Routing_filter_DYN == 0 ) goto error;

	errorcode = Filter_file( fd, tempfd, "ROUTING_FILTER",
		Routing_filter_DYN, Filter_options_DYN, job, &env, 0);
	if(errorcode)switch(errorcode){
		case 0: break;
		case JHOLD:
			Set_flag_value(&job->info,HOLD_TIME,time((void *)0) );
			errorcode = 0;
			break;
		case JREMOVE: break;
		default:
			SNPRINTF(error,errlen)"Get_route: incoming control filter '%s' failed '%s'",
				Incoming_control_filter_DYN, Server_status(errorcode) );
			errorcode = JFAIL;
			goto error;
	}
	if( tempfd > 0 ) close(tempfd); tempfd = -1;

	Free_line_list( &env );
	Get_file_image_and_split(tempfile,0,1,&env,Line_ends,0,0,0,1,0,0);
	Free_line_list(&job->destination);

	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if(!id){
		FATAL(LOG_ERR)
			_("Get_route: no identifier for '%s'"),
			Find_str_value(&job->info,HF_NAME,Value_sep) );
	}
	count = 0;
	for(i = 0; i < env.count; ++i ){
		s = env.list[i];
		if( safestrcasecmp(END,s) ){
			if( !isupper(cval(s))
				&& (t = safestrpbrk(s,Value_sep)) ){
				*t = '=';
			}
			Add_line_list(&job->destination,s,Value_sep,1,1);
		} else {
			if( (s = Find_str_value(&job->destination, DEST,Value_sep)) ){
				int n;
				DEBUG1("Get_route: destination '%s'", s );
				Set_flag_value(&job->destination,DESTINATION,count);
				n = Find_flag_value(&job->destination,COPIES,Value_sep);
				if( n < 0 ){
					Set_flag_value(&job->destination,COPIES,0);
				}
				SNPRINTF(buffer,sizeof(buffer))".%d",count+1);
				s = safestrdup2(id,buffer,__FILE__,__LINE__);
				Set_str_value(&job->destination,IDENTIFIER,s);
				if(s) free(s);
				Update_destination(job);
				++count;
			}
			Free_line_list(&job->destination);
		}
	}
	if( (s = Find_str_value(&job->destination, DEST,Value_sep)) ){
		int n;
		DEBUG1("Get_route: destination '%s'", s );
		Set_flag_value(&job->destination,DESTINATION,count);
		n = Find_flag_value(&job->destination,COPIES,Value_sep);
		if( n < 0 ){
			Set_flag_value(&job->destination,COPIES,0);
		}
		SNPRINTF(buffer,sizeof(buffer))".%d",count+1);
		s = safestrdup2(id,buffer,__FILE__,__LINE__);
		Set_str_value(&job->destination,IDENTIFIER,s);
		if(s) free(s);
		Update_destination(job);
		++count;
	}
	Free_line_list(&job->destination);
	Set_flag_value(&job->info,DESTINATIONS,count);
#ifdef ORIGINAL_DEBUG//JY@1020
	if(DEBUGL1)Dump_job("Get_route: final", job );
#endif

 error:
	Free_line_list(&info);
	Free_line_list(&dest);
	Free_line_list(&env);
	Free_line_list(&cf_line_list);
	return( errorcode );
}
#endif

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
"$Id: lpd_status.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "getopt.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "getprinter.h"
#include "getqueue.h"
#include "child.h"
#include "fileopen.h"
#include "sendreq.h"
#include "globmatch.h"
#include "permission.h"
#include "lockfile.h"
#include "errorcodes.h"

#include "lpd_jobs.h"
#include "lpd_status.h"

//extern char printerstatus[32];//JY1106

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Tue May  2 09:32:50 PDT 1995
 * 
 * Return status:
 * 	status has two formats: short and long
 * 
 * Status information is obtained from 3 places:
 * 1. The status file
 * 2. any additional progress files indicated in the status file
 * 3. job queue
 * 
 * The status file is maintained by the current unspooler process.
 * It updates this file with the following information:
 * 
 * PID of the unspooler process   [line 1]
 * active job and  status file name
 * active job and  status file name
 * 
 * Example 1:
 * 3012
 * cfa1024host status
 * 
 * Example 2:
 * 3015
 * cfa1024host statusfd2
 * cfa1026host statusfd1
 * 
 * Format of the information reporting:
 * 
 * 0        1         2         3         4         5         6         7
 * 12345678901234567890123456789012345678901234567890123456789012345678901234
 *  Rank   Owner/ID                   Class Job  Files               Size Time
 * 1      papowell@astart4+322          A  322 /tmp/hi                3 17:33:47
 * x     Sx                           SxSx    Sx                 Sx    Sx       X
 *                                                                              
 ***************************************************************************/

#define RANKW 7
#define OWNERW 29
#define CLASSW 2
#define JOBW 6
#define FILEW 18
#define SIZEW 6
#define TIMEW 8


int Job_status( int *sock, char *input )
{
	char *s, *t, *name, *hash_key;
	int displayformat, status_lines = 0, i, n;
	struct line_list l, listv;
	struct line_list done_list;
	char error[SMALLBUFFER], buffer[16];
	int db, dbflag;
	
	FILE *READSTATUSFILE;//JY1120
	char readbuffer[SMALLBUFFER];//JY1120
	char *str_index;//JY1120

#if !defined(JYWENG20031104status)
	if( input && *input ) ++input;//JY1114
	if(get_queue_name(input))
	{
		printf("QueueName is not LPRServer\n");
		send_ack_packet(sock, ACK_FAIL);//JY1120
		return(0);
	}
	else
		printf("QueueName is LPRServer\n");

	int prnstatus=1;
	char buffertosend[LARGEBUFFER];
	int fdPRNPARorUSB=0;/*JYWENG20031104*/
	if((check_par_usb_prn())== 1)
		fdPRNPARorUSB=open("/dev/usb/lp0",O_RDWR);
	else
		fdPRNPARorUSB=open("/dev/lp0",O_RDWR);

	if(fdPRNPARorUSB == 0)
	{
		printf("file descriptor not created\n");
		send_ack_packet(sock, ACK_FAIL);//JY1120
		return(0);	
	}
//	ioctl(fdPRNPARorUSB, 0x060b, &prnstatus);
//	if(prnstatus == 0)
//JY1120
	if((READSTATUSFILE=fopen("/var/state/printstatus.txt", "r")) == NULL)
	{
		printf("open /var/state/printstatus.txt failed!\n");
		send_ack_packet(sock, ACK_FAIL);//JY1120
		return(0);		
	}
	while( fgets(readbuffer, SMALLBUFFER, READSTATUSFILE) != NULL)
	{
		if((str_index = strstr(readbuffer, "PRINTER_STATUS=\"")))
		{
			str_index += 16;//moving to status
			strncpy(printerstatus, str_index, strlen(str_index) - 2 );	
		}
	}
//JY1120	

	SNPRINTF(buffertosend, sizeof(buffertosend))"Status: %s\n", printerstatus);
//	else
//		SNPRINTF(buffertosend, sizeof(buffertosend))"Status: Off line\n");
//	if( Write_fd_str( *sock, buffertosend ) < 0 ) cleanup(0);
	if( write( *sock, buffertosend, strlen(buffertosend) ) < 0 ) cleanup(0);
	exit(0);//JY1120
	
#endif


#ifdef REMOVE
	Init_line_list(&l);
	Init_line_list(&listv);
	Init_line_list(&done_list);
	db = Debug;
	dbflag = DbgFlag;

	Name = "Job_status";

	/* get the format */
	if( (s = safestrchr(input, '\n' )) ) *s = 0;
	displayformat = *input++;

	/*
	 * if we get a short/long request from these hosts,
	 * reverse the sense of question
	 */
	if( Reverse_lpq_status_DYN
		&& (displayformat == REQ_DSHORT || displayformat==REQ_DLONG)  ){
		Free_line_list(&l);
		Split(&l,Reverse_lpq_status_DYN,File_sep,0,0,0,0,0,0);
		if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
			DEBUGF(DLPQ1)("Job_status: reversing status sense");
			if( displayformat == REQ_DSHORT ){
				displayformat = REQ_DLONG;
			} else {
				displayformat = REQ_DSHORT;
			}
		}
		Free_line_list(&l);
	}
	/*
	 * we have a list of hosts with format of the form:
	 *  Key=list; Key=list;...
	 *  key is s for short, l for long
	 */
	DEBUGF(DLPQ1)("Job_status: Force_lpq_status_DYN '%s'", Force_lpq_status_DYN);
	if( Force_lpq_status_DYN ){
		Free_line_list(&listv);
		Split(&listv,Force_lpq_status_DYN,";",0,0,0,0,0,0);
		for(i = 0; i < listv.count; ++i ){
			s = listv.list[i];
			if( (t = safestrpbrk(s,File_sep)) ) *t++ = 0;
			Free_line_list(&l);
			Split(&l,t,Value_sep,0,0,0,0,0,0);
			DEBUGF(DLPQ1)("Job_status: Force_lpq_status '%s'='%s'", s,t);
			if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
				DEBUGF(DLPQ1)("Job_status: forcing status '%s'", s);
				if( safestrcasecmp(s,"s") == 0 ){
					displayformat = REQ_DSHORT;
				} else if( safestrcasecmp(s,"l") == 0 ){
					displayformat = REQ_DLONG;
				}
				status_lines = Short_status_length_DYN;
				break;
			}
		}
		Free_line_list(&l);
		Free_line_list(&listv);
	}

	/*
	 * check for short status to be returned
	 */

	if( Return_short_status_DYN && displayformat == REQ_DLONG ){
		Free_line_list(&l);
		Split(&l,Return_short_status_DYN,File_sep,0,0,0,0,0,0);
		if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
			status_lines = Short_status_length_DYN;
			DEBUGF(DLPQ1)("Job_status: truncating status to %d",
				status_lines);
		}
		Free_line_list(&l);
	}

	DEBUGF(DLPQ1)("Job_status: doing '%s'", input );
	Free_line_list(&l);
	Split(&l,input,Whitespace,0,0,0,0,0,0);
	if( l.count == 0 ){
		SNPRINTF( error, sizeof(error)) "zero length command line");
		goto error;
	}

	/* save l.list[0] */
	name = l.list[0];
	
	if( (s = Is_clean_name( name )) ){
		SNPRINTF( error, sizeof(error))
			_("printer '%s' has illegal character at '%s' in name"), name, s );
		goto error;
	}

	Set_DYN(&Printer_DYN,name);
	setproctitle( "lpd %s '%s'", Name, name );
	SNPRINTF(buffer,sizeof(buffer))"%d",displayformat);
	l.list[0] = buffer;

	/* we have the hash key */
	hash_key = Join_line_list_with_sep(&l,"_");
	for( s = hash_key; (s = strpbrk(s,Whitespace)); ) *s = '_';
	
	DEBUGF(DLPQ1)("Job_status: arg '%s'", s );
	/* now we put back the l.list[0] value */

	l.list[0] = name;
	/* free the values l.list[0] */
	Remove_line_list( &l, 0 );
	name = Printer_DYN;

	if( l.count && (s = l.list[0]) && s[0] == '-' ){
		DEBUGF(DLPQ1)("Job_status: arg '%s'", s );
		Free_line_list(&listv);
		Split(&listv,s+1,Arg_sep,1,Value_sep,1,1,0,0);
		Remove_line_list( &l, 0 );
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ1)Dump_line_list( "Job_status: args", &listv );
#endif
		if( (n = Find_flag_value(&listv,"lines",Value_sep)) ) status_lines = n;
		DEBUGF(DLPQ1)("Job_status: status_lines '%d'", status_lines );
		Free_line_list(&listv);
	}
	if( safestrcasecmp( name, ALL ) ){
		DEBUGF(DLPQ1)("Job_status: checking printcap entry '%s'",  name );
		Get_queue_status( &l, sock, displayformat, status_lines,
			&done_list, Max_status_size_DYN, hash_key );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		
		Get_all_printcap_entries();
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN, All_line_list.list[i] );
			Debug = db;
			DbgFlag = dbflag;
			Get_queue_status( &l, sock, displayformat, status_lines,
				&done_list, Max_status_size_DYN, hash_key );
		}
	}
	Free_line_list( &l );
	Free_line_list( &listv );
	Free_line_list( &done_list );
	DEBUGF(DLPQ3)("Job_status: DONE" );
	return(0);

 error:
	DEBUGF(DLPQ2)("Job_status: error msg '%s'", error );
	i = safestrlen(error);
	if( (i = safestrlen(error)) >= (int)sizeof(error)-2 ){
		i = sizeof(error) - 2;
	}
	error[i++] = '\n';
	error[i] = 0;
	Free_line_list( &l );
	Free_line_list( &listv );
	Free_line_list( &done_list );
	if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
	DEBUGF(DLPQ3)("Job_status: done" );
#endif
	return(0);
}

#ifdef REMOVE
/***************************************************************************
 * void Get_queue_status
 * sock - used to send information
 * displayformat - REQ_DSHORT, REQ_DLONG, REQ_VERBOSE
 * tokens - arguments
 *  - get the printcap entry (if any)
 *  - check the control file for current status
 *  - find and report the spool queue entries
 ***************************************************************************/
void Get_queue_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size, char *hash_key )
{
	char msg[SMALLBUFFER], buffer[SMALLBUFFER], error[SMALLBUFFER],
		number[LINEBUFFER], header[LARGEBUFFER];
	char sizestr[SIZEW+TIMEW+32];
	char *pr, *s, *t, *path, *identifier,
		*jobname, *joberror, *class, *priority, *d_identifier,
		*job_time, *d_error, *d_dest, *openname, *hf_name, *filenames,
		*tempfile = 0, *file = 0, *end_of_name;
	struct line_list outbuf, info, lineinfo, cache, cache_info;
	int status = 0, len, ix, nx, flag, count, held, move,
		server_pid, unspooler_pid, fd, nodest,
		printable, dcount, destinations = 0,
		d_copies, d_copy_done, permission, jobnumber, db, dbflag,
		matches, tempfd, savedfd, lockfd, delta, err, cache_index,
		total_held, total_move, jerror, jdone;
	double jobsize;
	struct stat statb;
	struct job job;
	time_t modified = 0;
	time_t timestamp = 0;
	time_t now = time( (void *)0 );

	cache_index = -1;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUG1("Get_queue_status: sock fd %d, checking '%s'", *sock, Printer_DYN );
	if(DEBUGL1)Dump_line_list( "Get_queue_status: done_list", done_list );
#endif

	/* set printer name and printcap variables */

	Init_job(&job);
	Init_line_list(&info);
	Init_line_list(&lineinfo);
	Init_line_list(&outbuf);
	Init_line_list(&cache);
	Init_line_list(&cache_info);
	/* for caching */
	tempfile = 0; 
	savedfd = tempfd = lockfd = -1;

	Check_max(tokens,2);
	tokens->list[tokens->count] = 0;
	msg[0] = 0;
	header[0] = 0;
	error[0] = 0;
	pr = s = 0;

	safestrncpy(buffer,Printer_DYN);
	status = Setup_printer( Printer_DYN, error, sizeof(error), 0);
	if( status ){
		if( error[0] == 0 ){
			SNPRINTF(error,sizeof(error))"Nonexistent printer '%s'", Printer_DYN);
		}
		goto error;
	}

	db = Debug;
	dbflag = DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if( !s ) s = New_debug_DYN;
	Parse_debug( s, 0 );
	if( !(DLPQMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbflag;
	} else {
		int odb, odbf;
		odb = Debug;
		odbf = DbgFlag;
		Debug = db;
		DbgFlag = dbflag;
		if( Log_file_DYN ){
			fd = Trim_status_file( -1, Log_file_DYN, Max_log_file_size_DYN,
				Min_log_file_size_DYN );
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
				close(fd);
			}
		}
		Debug = odb;
		DbgFlag = odbf;
	}

	DEBUGF(DLPQ3)("Get_queue_status: sock fd %d, Setup_printer status %d '%s'", *sock, status, error );
	/* set up status */
	if( Find_exists_value(done_list,Printer_DYN,Value_sep ) ){
		return;
	}
	Add_line_list(done_list,Printer_DYN,Value_sep,1,1);

	/* check for permissions */

	Perm_check.service = 'Q';
	Perm_check.printer = Printer_DYN;

	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 );
	DEBUGF(DLPQ1)("Job_status: permission '%s'", perm_str(permission));
	if( permission == P_REJECT ){
		SNPRINTF( error, sizeof(error))
			_("%s: no permission to show status"), Printer_DYN );
		goto error;
	}

	/* check to see if we have any cached information */
	if( Lpq_status_cached_DYN > 0 && Lpq_status_file_DYN ){
		fd = -1;
		do{
			DEBUGF(DLPQ1)("Job_status: getting lock on '%s'", Lpq_status_file_DYN);
			lockfd = Checkwrite( Lpq_status_file_DYN, &statb, O_RDWR, 1, 0 );
			if( lockfd < 0 ){
				LOGERR_DIE(LOG_INFO)"Get_queue_status: cannot open '%s'",
				Lpq_status_file_DYN);
			}
			if( Do_lock(lockfd, 0) < 0 ){
				DEBUGF(DLPQ1)("Get_queue_status: did not get lock");
				Do_lock(lockfd, 1);
				DEBUGF(DLPQ1)("Get_queue_status: lock released");
				close(lockfd); lockfd = -1;
			}
		}while( lockfd < 0 );
		DEBUGF(DLPQ1)("Get_queue_status: lock succeeded");
		Free_line_list(&cache);
		Get_fd_image_and_split(lockfd, 0,0,&cache,Line_ends,0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status- cache", &cache );
		DEBUGF(DLPQ3)("Get_queue_status: cache hash_key '%s'", hash_key );
#endif
		file = 0;
		nx = -1;
		if( cache.count < Lpq_status_cached_DYN ){
			Check_max(&cache,Lpq_status_cached_DYN-cache.count);
			for( ix = cache.count; ix < Lpq_status_cached_DYN; ++ix ){
				cache.list[ix] = 0;
			}
			cache.count = ix;
		}
		for( ix = 0; file == 0 && ix < cache.count; ++ix ){
			if( (s = cache.list[ix]) ){
				if( (t = safestrchr(s,'=')) ){
					*t = 0;
					if( !strcmp(hash_key,s) ){
						file = t+1;
						cache_index = ix;
					}
					*t = '=';
				}
			}
		}
		/* if we have a file name AND it is not stale then we use it */
		DEBUGF(DLPQ3)("Get_queue_status: found in cache '%s'", file );
		fd = -1;
		if( file ){
			Split(&cache_info,file,Arg_sep,1,Value_sep,1,1,0,0);
			file = Find_str_value(&cache_info,FILENAMES,Value_sep);
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status: cache_info", &cache_info );
#endif
		if( file && (fd = Checkread( file, &statb )) > 0 ){
			modified = statb.st_mtime;
			delta = now - modified;
			if( Lpq_status_stale_DYN && delta > Lpq_status_stale_DYN ){
				/* we cannot use it */
				close(fd); fd = -1;
			}  
		}
		if( fd > 0 ){
			modified = 0;
			if( Queue_status_file_DYN && stat(Queue_status_file_DYN,&statb) == 0 ){
				modified = statb.st_mtime;
			}
			timestamp = Find_flag_value(&cache_info,QUEUE_STATUS_FILE,Value_sep);
			delta = modified - timestamp;
			DEBUGF(DLPQ3)("Get_queue_status: queue status '%s', modified %lx, timestamp %lx, delta %d",
				Queue_status_file_DYN, (long)(modified), (long)(timestamp), delta );
			if( delta > Lpq_status_interval_DYN ){
				/* we need to refresh the data */
				close(fd); fd = -1;
			}
		}

		if( fd > 0 ){
			if( Status_file_DYN && stat(Status_file_DYN,&statb) == 0 ){
				modified = statb.st_mtime;
			}
			timestamp = Find_flag_value(&cache_info,PRSTATUS,Value_sep);
			delta = modified - timestamp;
			DEBUGF(DLPQ3)("Get_queue_status: pr status '%s', modified %lx, timestamp %lx, delta %d",
				Status_file_DYN, (long)(modified), (long)(timestamp), delta );
			if( delta > Lpq_status_interval_DYN ){
				/* we need to refresh the data */
				close(fd); fd = -1;
			}
		}

		if( fd > 0 ){
			DEBUGF(DLPQ3)("Get_queue_status: reading cached status from fd '%d'", fd );
			/* We can read the status from the cached data */
			while( (ix = read( fd, buffer, sizeof(buffer)-1 )) > 0 ){
				if( write( *sock, buffer, ix ) < 0 ){
					cleanup(0);
				}
			}
			close(fd); fd = -1;
			goto remote;
		}
		/* OK, we have to cache the status in a file */
		tempfd = Make_temp_fd( &tempfile );
		savedfd = *sock;
		*sock = tempfd;
	}

	end_of_name = 0;
	if( displayformat != REQ_DSHORT ){
		SNPRINTF( header, sizeof(header)) "%s: ",
			Server_queue_name_DYN?"Server Printer":"Printer" );
	}
	len = safestrlen(header);
	SNPRINTF( header+len, sizeof(header)-len) "%s@%s",
		Printer_DYN, Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
	if( safestrcasecmp( buffer, Printer_DYN ) ){
		len = safestrlen(header);
		SNPRINTF( header+len, sizeof(header)-len) _(" (originally %s)"), buffer );
	}
	end_of_name = header+safestrlen(header);

	if( status ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			safestrncat( header, _("\n Error: ") );
			len = safestrlen( header );
		}
		if( error[0] ){
			SNPRINTF( header+len, sizeof(header)-len)
				_(" - %s"), error );
		} else if( !Spool_dir_DYN ){
			SNPRINTF( header+len, sizeof(header)-len)
				_(" - printer %s@%s not in printcap"), Printer_DYN,
					Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" - printer %s@%s has bad printcap entry"), Printer_DYN,
					Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
		}
		safestrncat( header, "\n" );
		DEBUGF(DLPQ3)("Get_queue_status: forward header '%s'", header );
		if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
		header[0] = 0;
		goto done;
	}
	if( displayformat == REQ_VERBOSE ){
		safestrncat( header, "\n" );
		if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
		header[0] = 0;
	}

	/* get the spool entries */
	Free_line_list( &outbuf );
	Scan_queue( &Spool_control, &Sort_order, &printable,&held,&move,0,0,0,0,0 );
	/* check for done jobs, remove any if there are some */
	if( Remove_done_jobs() ){
		Scan_queue( &Spool_control, &Sort_order, &printable,&held,&move,0,0,0,0,0 );
	}

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DLPQ3)("Get_queue_status: total files %d", Sort_order.count );
	DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status- Sort_order", &Sort_order );
#endif


	/* set up the short format for folks */

	if( displayformat == REQ_DLONG && Sort_order.count > 0 ){
		/*
		 Rank  Owner/ID  Class Job Files   Size Time
		*/
		Add_line_list(&outbuf,
" Rank   Owner/ID                  Class Job Files                 Size Time"
		,0,0,0);
	}
	error[0] = 0;

	matches = 0;
	total_held = 0;
	total_move = 0;
	for( count = 0; count < Sort_order.count; ++count ){
		int printable, held, move;
		printable = held = move = 0;
		Free_job(&job);
		Get_hold_file(&job, Sort_order.list[count] );
		Job_printable(&job,&Spool_control, &printable,&held,&move,&jerror,&jdone);
		DEBUGF(DLPQ3)("Get_queue_status: printable %d, held %d, move %d, error %d, done %d",
			printable, held, move, jerror, jdone );
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ4)Dump_job("Get_queue_status - info", &job );
#endif
		if( job.info.count == 0 ) continue;

		if( tokens->count && Patselect( tokens, &job.info, 0) ){
			continue;
		}

		number[0] = 0;
		error[0] = 0;
		msg[0] = 0;
		nodest = 0;
		s = Find_str_value(&job.info,PRSTATUS,Value_sep);
		if( s == 0 ){
			SNPRINTF(number,sizeof(number))"%d",count+1);
		} else {
			SNPRINTF(number,sizeof(number))"%s",s);
		}
		identifier = Find_str_value(&job.info,IDENTIFIER,Value_sep);
		if( identifier == 0 ){
			identifier = Find_str_value(&job.info,LOGNAME,Value_sep);
		}
		if( identifier == 0 ){
			identifier = "???";
		}
		priority = Find_str_value(&job.info,PRIORITY,Value_sep);
		class = Find_str_value(&job.info,CLASS,Value_sep);
		jobname = Find_str_value(&job.info,JOBNAME,Value_sep);
		filenames = Find_str_value(&job.info,FILENAMES,Value_sep);
		jobnumber = Find_decimal_value(&job.info,NUMBER,Value_sep);
		joberror = Find_str_value(&job.info,ERROR,Value_sep);
		jobsize = Find_double_value(&job.info,SIZE,Value_sep);
		job_time = Find_str_value(&job.info,JOB_TIME,Value_sep );
		destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep);

		openname = Find_str_value(&job.info,OPENNAME,Value_sep);
		if( !openname ){
			openname = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
		}
		if( !openname ){
			DEBUGF(DLPQ4)("Get_queue_status: no openname or transfername");
			continue;
		}
		hf_name = Find_str_value(&job.info,HF_NAME,Value_sep);
		if( !hf_name ){
			DEBUGF(DLPQ4)("Get_queue_status: no hf_name");
			continue;
		}

		/* we report this jobs status */

		DEBUGF(DLPQ3)("Get_queue_status: joberror '%s'", joberror );
		DEBUGF(DLPQ3)("Get_queue_status: class '%s', priority '%s'",
			class, priority );

		if( (Class_in_status_DYN && class) || priority == 0 ){
			priority = class;
		}

		if( displayformat == REQ_DLONG ){
			SNPRINTF( msg, sizeof(msg))
				"%-*s %-*s ", RANKW-1, number, OWNERW-1, identifier );
			while( (len = safestrlen(msg)) > (RANKW+OWNERW)
				&& isspace(cval(msg+len-1)) && isspace(cval(msg+len-2)) ){
				msg[len-1] = 0;
			}
			SNPRINTF( buffer, sizeof(buffer)) "%-*s %*d ",
				CLASSW-1,priority, JOBW-1,jobnumber);
			DEBUGF(DLPQ3)("Get_queue_status: msg len %d '%s', buffer %d, '%s'",
				safestrlen(msg),msg, safestrlen(buffer), buffer );
			DEBUGF(DLPQ3)("Get_queue_status: RANKW %d, OWNERW %d, CLASSW %d, JOBW %d",
				RANKW, OWNERW, CLASSW, JOBW );
			s = buffer;
			while( safestrlen(buffer) > CLASSW+JOBW && (s = safestrchr(s,' ')) ){
				if( cval(s+1) == ' ' ){
					memmove(s,s+1,safestrlen(s)+1);
				} else {
					++s;
				}
			}
			s = msg+safestrlen(msg)-1;
			while( safestrlen(msg) + safestrlen(buffer) > RANKW+OWNERW+CLASSW+JOBW ){
				if( cval(s) == ' ' && cval(s-1) == ' ' ){
					*s-- = 0;
				} else {
					break;
				}
			}
			s = buffer;
			while( safestrlen(msg) + safestrlen(buffer) > RANKW+OWNERW+CLASSW+JOBW
				&& (s = safestrchr(s,' ')) ){
				if( cval(s+1) == ' ' ){
					memmove(s,s+1,safestrlen(s)+1);
				} else {
					++s;
				}
			}
			len = safestrlen(msg);

			SNPRINTF(msg+len, sizeof(msg)-len)"%s",buffer);
			if( joberror ){
				len = safestrlen(msg);
					SNPRINTF(msg+len,sizeof(msg)-len)
					"ERROR: %s", joberror );
			} else {
				DEBUGF(DLPQ3)("Get_queue_status: jobname '%s'", jobname );

				len = safestrlen(msg);
				SNPRINTF(msg+len,sizeof(msg)-len)"%-s",jobname?jobname:filenames);

				DEBUGF(DLPQ3)("Get_queue_status: jobtime '%s'", job_time );
				job_time = Time_str(1, Convert_to_time_t(job_time));
				if( !Full_time_DYN && (s = safestrchr(job_time,'.')) ) *s = 0;

				{
					char jobb[32];
					SNPRINTF(jobb,sizeof(jobb)) "%0.0f", jobsize );
					SNPRINTF( sizestr, sizeof(sizestr)) "%*s %-s",
						SIZEW-1,jobb, job_time );
				}

				len = Max_status_line_DYN;
				if( len >= (int)sizeof(msg)) len = sizeof(msg)-1;
				len = len-safestrlen(sizestr);
				if( len > 0 ){
					/* pad with spaces */
					for( nx = safestrlen(msg); nx < len; ++nx ){
						msg[nx] = ' ';
					}
					msg[nx] = 0;
				}
				/* remove spaces if necessary */
				while( safestrlen(msg) + safestrlen(sizestr) > Max_status_line_DYN ){
					if( isspace( cval(sizestr) ) ){
						memmove(sizestr, sizestr+1, safestrlen(sizestr)+1);
					} else {
						s = msg+safestrlen(msg)-1;
						if( isspace(cval(s)) && isspace(cval(s-1)) ){
							s[0] = 0;
						} else {
							break;
						}
					}
				}
				if( safestrlen(msg) + safestrlen(sizestr) >= Max_status_line_DYN ){
					len = Max_status_line_DYN - safestrlen(sizestr);
					msg[len-1] = ' ';
					msg[len] = 0;
				}
				strcpy( msg+safestrlen(msg), sizestr );
			}

			if( Max_status_line_DYN < (int)sizeof(msg) ) msg[Max_status_line_DYN] = 0;

			DEBUGF(DLPQ3)("Get_queue_status: adding '%s'", msg );
			Add_line_list(&outbuf,msg,0,0,0);
			DEBUGF(DLPQ3)("Get_queue_status: destinations '%d'", destinations );
			if( nodest == 0 && destinations ){
				for( dcount = 0; dcount < destinations; ++dcount ){
					if( Get_destination( &job, dcount ) ) continue;
#ifdef ORIGINAL_DEBUG//JY@1020
					DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status: destination",
						&job.destination);
#endif
					d_error =
						Find_str_value(&job.destination,ERROR,Value_sep);
					d_dest =
						Find_str_value(&job.destination,DEST,Value_sep);
					d_copies = 
						Find_flag_value(&job.destination,COPIES,Value_sep);
					d_copy_done = 
						Find_flag_value(&job.destination,COPY_DONE,Value_sep);
					d_identifier =
						Find_str_value(&job.destination,IDENTIFIER,Value_sep);
					s = Find_str_value(&job.destination, PRSTATUS,Value_sep);
					if( !s ) s = "";
					SNPRINTF(number, sizeof(number))" - %-8s", s );
					SNPRINTF( msg, sizeof(msg))
						"%-*s %-*s ", RANKW, number, OWNERW, d_identifier );
					len = safestrlen(msg);
					SNPRINTF(msg+len, sizeof(msg)-len) " ->%s", d_dest );
					if( d_copies > 1 ){
						len = safestrlen( msg );
						SNPRINTF( msg+len, sizeof(msg)-len)
							_(" <cpy %d/%d>"), d_copy_done, d_copies );
					}
					if( d_error ){
						len = safestrlen(msg);
						SNPRINTF( msg+len, sizeof(msg)-len) " ERROR: %s", d_error );
					}
					Add_line_list(&outbuf,msg,0,0,0);
				}
			}
			DEBUGF(DLPQ3)("Get_queue_status: after dests" );
		} else if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header, sizeof(header))
				_(" Job: %s"), identifier );
			SNPRINTF( msg, sizeof(msg)) _("%s status= %s"),
				header, number );
			Add_line_list(&outbuf,msg,0,0,0);
			SNPRINTF( msg, sizeof(msg)) _("%s size= %0.0f"),
				header, jobsize );
			Add_line_list(&outbuf,msg,0,0,0);
			SNPRINTF( msg, sizeof(msg)) _("%s time= %s"),
				header, job_time );
			Add_line_list(&outbuf,msg,0,0,0);
			if( joberror ){
				SNPRINTF( msg, sizeof(msg)) _("%s error= %s"),
						header, joberror );
				Add_line_list(&outbuf,msg,0,0,0);
			}
			SNPRINTF( msg, sizeof(msg)) _("%s CONTROL="), header );
			Add_line_list(&outbuf,msg,0,0,0);
			s = Get_file_image(openname,0);
			Add_line_list(&outbuf,s,0,0,0);
			if( s ) free(s); s = 0;

			SNPRINTF( msg, sizeof(msg)) _("%s HOLDFILE="), header );
			Add_line_list(&outbuf,msg,0,0,0);
			s = Get_file_image(hf_name,0);
			Add_line_list(&outbuf,s,0,0,0);
			if( s ) free(s); s = 0;
		} else if( displayformat == REQ_DSHORT ){
			if( printable ){
				++matches;
			} else if( held ){
				++total_held;
			} else if( move ){
				++total_move;
			}
		}
	}
	DEBUGF(DLPQ3)("Get_queue_status: matches %d", matches );
	/* this gives a short 1 line format with minimum info */
	if( displayformat == REQ_DSHORT ){
		len = safestrlen( header );
		SNPRINTF( header+len, sizeof(header)-len) _(" %d job%s"),
			matches, (matches == 1)?"":"s" );
		if( total_held ){
			len = safestrlen( header );
			SNPRINTF( header+len, sizeof(header)-len) _(" (%d held)"), 
				total_held );
		}
		if( total_move ){
			len = safestrlen( header );
			SNPRINTF( header+len, sizeof(header)-len) _(" (%d move)"), 
				total_move );
		}
	}
	len = safestrlen( header );

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPQ4)Dump_line_list("Get_queue_status: job status",&outbuf);

	DEBUGF(DLPQ3)(
		"Get_queue_status: RemoteHost_DYN '%s', RemotePrinter_DYN '%s', Lp '%s'",
		RemoteHost_DYN, RemotePrinter_DYN, Lp_device_DYN );
#endif

	if( displayformat != REQ_DSHORT ){
		s = 0;
		if( (s = Comment_tag_DYN) == 0 ){
			if( (nx = PC_alias_line_list.count) > 1 ){
				s = PC_alias_line_list.list[nx-1];
			}
		}
		if( s ){
			s = Fix_str(s);
			len = safestrlen( header );
			if( displayformat == REQ_VERBOSE ){
				SNPRINTF( header+len, sizeof(header)-len) _(" Comment: %s"), s );
			} else {
				SNPRINTF( header+len, sizeof(header)-len) " '%s'", s );
			}
			if(s) free(s); s = 0;
		}
	}

	len = safestrlen( header );
	if( displayformat == REQ_VERBOSE ){
		SNPRINTF( header+len, sizeof(header)-len)
			_("\n Printing: %s\n Aborted: %s\n Spooling: %s"),
				Pr_disabled(&Spool_control)?"yes":"no",
				Pr_aborted(&Spool_control)?"yes":"no",
				Sp_disabled(&Spool_control)?"yes":"no");
	} else if( displayformat == REQ_DLONG || displayformat == REQ_DSHORT ){
		flag = 0;
		if( Pr_disabled(&Spool_control) || Sp_disabled(&Spool_control) || Pr_aborted(&Spool_control) ){
			SNPRINTF( header+len, sizeof(header)-len) " (" );
			len = safestrlen( header );
			if( Pr_disabled(&Spool_control) ){
				SNPRINTF( header+len, sizeof(header)-len) "%s%s",
					flag?", ":"", "printing disabled" );
				flag = 1;
				len = safestrlen( header );
			}
			if( Pr_aborted(&Spool_control) ){
				SNPRINTF( header+len, sizeof(header)-len) "%s%s",
					flag?", ":"", "printing aborted" );
				flag = 1;
				len = safestrlen( header );
			}
			if( Sp_disabled(&Spool_control) ){
				SNPRINTF( header+len, sizeof(header)-len) "%s%s",
					flag?", ":"", "spooling disabled" );
				len = safestrlen( header );
			}
			SNPRINTF( header+len, sizeof(header)-len) ")" );
			len = safestrlen( header );
		}
	}

	/*
	 * check to see if this is a server or subserver.  If it is
	 * for subserver,  then you can forget starting it up unless started
	 * by the server.
	 */
	if( (s = Server_names_DYN) || (s = Destinations_DYN) ){
		Split( &info, s, File_sep, 0,0,0,0,0,0);
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			if ( Server_names_DYN ) {
				s = "Subservers";
			} else {
				s = "Destinations";
			}
			SNPRINTF( header+len, sizeof(header)-len)
			_("\n %s: "), s );
		} else {
			if ( Server_names_DYN ) {
				s = "subservers";
			} else {
				s = "destinations";
			}
			SNPRINTF( header+len, sizeof(header)-len)
			_(" (%s"), s );
		}
		for( ix = 0; ix < info.count; ++ix ){
			len = safestrlen( header );
			SNPRINTF( header+len, sizeof(header)-len)
			"%s%s", (ix > 0)?", ":" ", info.list[ix] );
		}
		Free_line_list( &info );
		if( displayformat != REQ_VERBOSE ){
			safestrncat( header, ") " );
		}
	} else if( (s = Frwarding(&Spool_control)) ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Redirected_to: %s"), s );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (redirect %s)"), s );
		}
	} else if( RemoteHost_DYN && RemotePrinter_DYN ){
		len = safestrlen( header );
		s = Frwarding(&Spool_control);
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				"\n Destination: %s@%s", RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
			_(" (dest %s@%s)"), RemotePrinter_DYN, RemoteHost_DYN );
		}
	}
	if( Server_queue_name_DYN ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Serving: %s"), Server_queue_name_DYN );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (serving %s)"), Server_queue_name_DYN );
		}
	}
	if( (s = Clsses(&Spool_control)) ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Classes: %s"), s );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (classes %s)"), s );
		}
	}
	if( (Hld_all(&Spool_control)) ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Hold_all: on") );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (holdall)"));
		}
	}
	if( Auto_hold_DYN ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Auto_hold: on") );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (autohold)"));
		}
	}

	if( (s = Find_str_value( &Spool_control,MSG,Value_sep )) ){
		len = safestrlen( header );
		if( displayformat == REQ_VERBOSE ){
			SNPRINTF( header+len, sizeof(header)-len)
				_("\n Message: %s"), s );
		} else {
			SNPRINTF( header+len, sizeof(header)-len)
				_(" (message: %s)"), s );
		}
	}
	safestrncat( header, "\n" );
	if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
	header[0] = 0;

	if( displayformat == REQ_DSHORT ) goto remote;

	/* now check to see if there is a server and unspooler process active */
	path = Make_pathname( Spool_dir_DYN, Queue_lock_file_DYN );
	server_pid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		server_pid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	DEBUGF(DLPQ3)("Get_queue_status: checking server pid %d", server_pid );
	free(path);
	if( server_pid > 0 && kill( server_pid, 0 ) ){
		DEBUGF(DLPQ3)("Get_queue_status: server %d not active", server_pid );
		server_pid = 0;
	}

	path = Make_pathname( Spool_dir_DYN, Queue_unspooler_file_DYN );
	unspooler_pid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		unspooler_pid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	if(path) free(path); path=0;
	DEBUGF(DLPQ3)("Get_queue_status: checking unspooler pid %d", unspooler_pid );
	if( unspooler_pid > 0 && kill( unspooler_pid, 0 ) ){
		DEBUGF(DLPQ3)("Get_queue_status: unspooler %d not active", unspooler_pid );
		unspooler_pid = 0;
	}

	if( printable == 0 ){
		safestrncpy( msg, _(" Queue: no printable jobs in queue\n") );
	} else {
		/* check to see if there are files and no spooler */
		SNPRINTF( msg, sizeof(msg)) _(" Queue: %d printable job%s\n"),
			printable, printable > 1 ? "s" : "" );
	}
	if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	if( held ){
		SNPRINTF( msg, sizeof(msg)) 
		_(" Holding: %d held jobs in queue\n"), held );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	}

	msg[0] = 0;
	if( count && server_pid == 0 ){
		safestrncpy(msg, _(" Server: no server active") );
	} else if( server_pid ){
		len = safestrlen(msg);
		SNPRINTF( msg+len, sizeof(msg)-len) _(" Server: pid %d active"),
			server_pid );
	}
	if( unspooler_pid ){
		if( msg[0] ){
			safestrncat( msg, (displayformat == REQ_VERBOSE )?", ":"\n");
		}
		len = safestrlen(msg);
		SNPRINTF( msg+len, sizeof(msg)-len) _(" Unspooler: pid %d active"),
			unspooler_pid );
	}
	if( msg[0] ){
		safestrncat( msg, "\n" );
	}
	if( msg[0] ){
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	}
	msg[0] = 0;

	if( displayformat == REQ_VERBOSE ){
		SNPRINTF( msg, sizeof(msg)) _("%s SPOOLCONTROL=\n"), header );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
		msg[0] = 0;
		for( ix = 0; ix < Spool_control.count; ++ix ){
			s = safestrdup3("   ",Spool_control.list[ix],"\n",__FILE__,__LINE__);
			if( Write_fd_str( *sock, s ) < 0 ) cleanup(0);
			free(s);
		}
	}

	/*
	 * get the last status of the spooler
	 */
	Print_status_info( sock, Queue_status_file_DYN,
		_(" Status: "), status_lines, max_size );

	if( Status_file_DYN ){
		Print_status_info( sock, Status_file_DYN,
			_(" Filter_status: "), status_lines, max_size );
	}

	s = Join_line_list(&outbuf,"\n");
	if( s ){
		if( Write_fd_str(*sock,s) < 0 ) cleanup(0);
		free(s);
	}
	Free_line_list(&outbuf);

 remote:
	if( tempfd > 0 ){
		/* we send the generated status back to the user */
		*sock = savedfd;
		DEBUGF(DLPQ3)("Get_queue_status: reporting created status" );
		if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
			LOGERR_DIE(LOG_INFO)"Get_queue_status: lseek of '%s' failed",
				tempfile );
		}
		while( (ix = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
			if( write( *sock, buffer, ix ) < 0 ){
				break;
			}
		}
		close(tempfd); tempfd = -1;
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status- cache", &cache );
		/* now we update the cached information */
		DEBUGF(DLPQ3)("Get_queue_status: hash_key '%s', cache_index %d",
			hash_key, cache_index );
#endif
		modified = 0;
		nx = -1;
		for( ix = 0; cache_index < 0 && ix < cache.count; ++ix ){
			s = cache.list[ix];
			DEBUGF(DLPQ3)("Get_queue_status: [%d] '%s'", ix, s );
			Free_line_list(&cache_info);
			if( s && (t = strchr(s,'=')) ){
				Split(&cache_info,t+1,Arg_sep,1,Value_sep,1,1,0,0);
				if( (file = Find_str_value(&cache_info,FILENAMES,Value_sep)) ){
					/* we need to get the age of the file */
					if( stat( file,&statb ) ){
						/* the file is not there */
						cache_index = ix;
					} else if( modified == 0 || statb.st_mtime < modified ){
						nx = ix;
						modified = statb.st_mtime;
					}
				} else {
					cache_index = ix;
				}
			} else {
				DEBUGF(DLPQ3)("Get_queue_status: end of list [%d]", ix );
				/* end of the list */
				cache_index = ix;
			}
		}
		DEBUGF(DLPQ3)("Get_queue_status: cache_index %d", cache_index );
		if( cache_index < 0 ) cache_index = nx;
		DEBUGF(DLPQ3)("Get_queue_status: using cache_index %d", cache_index );
		if( cache_index < 0 ){
			FATAL(LOG_INFO)"Get_queue_status: cache entry not found");
		}
		SNPRINTF(buffer,sizeof(buffer))"%s.%d", Lpq_status_file_DYN,cache_index);

		Free_line_list(&cache_info);
		Set_str_value(&cache_info,FILENAMES,buffer);

		modified = 0;
		if( Queue_status_file_DYN && stat(Queue_status_file_DYN,&statb) == 0 ){
			modified = statb.st_mtime;
		}
		Set_flag_value(&cache_info,QUEUE_STATUS_FILE,modified);
			
		modified = 0;
		if( Status_file_DYN && stat(Status_file_DYN,&statb) == 0 ){
			modified = statb.st_mtime;
		}
		Set_flag_value(&cache_info,PRSTATUS,modified);
		s = Join_line_list(&cache_info,",");

		/* now set up the new values */
		if( cache.list[cache_index] ) free( cache.list[cache_index]); cache.list[cache_index] = 0;
		cache.list[cache_index] = safestrdup3(hash_key,"=",s,__FILE__,__LINE__);
		if( s ) free(s); s = 0;

#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status- new cache", &cache );
#endif
		if( rename( tempfile, buffer ) ){
			err = errno;
			unlink( Lpq_status_file_DYN );
			errno = err;
			LOGERR_DIE(LOG_INFO)"Get_queue_status: rename of '%s' to '%s' failed",
				tempfile, buffer );
		}
		s = Join_line_list( &cache,"\n" );
		if( lseek( lockfd, 0, SEEK_SET) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_queue_status: lseek failed write file '%s'", Lpq_status_file_DYN);
		}
		if( ftruncate( lockfd, 0 ) ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_queue_status: ftruncate failed file '%s'", Lpq_status_file_DYN);
		}
		if( Write_fd_str( lockfd, s ) < 0 ){
			unlink( Lpq_status_file_DYN );
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_queue_status: write failed file '%s'", Lpq_status_file_DYN);
		}
		if(s) free(s); s = 0;
		close(lockfd);
		
#if 0
		tempfd = Make_temp_fd( &tempfile );
		if( Write_fd_str( tempfd, s ) < 0 ){
			err = errno;
			unlink( Lpq_status_file_DYN );
			LOGERR_DIE(LOG_INFO)"Get_queue_status: write to '%s' failed",
				tempfile );
			errno = err;
			cleanup(0);
		}
		close(tempfd); tempfd = -1;
		if(s) free(s); s = 0;
		if( rename( tempfile, Lpq_status_file_DYN ) ){
			err = errno;
			unlink( Lpq_status_file_DYN );
			errno = err;
			LOGERR_DIE(LOG_INFO)"Get_queue_status: rename of '%s' to '%s' failed",
				tempfile, Lpq_status_file_DYN );
		}
#endif
		Free_line_list(&cache_info);
		Free_line_list(&cache);
		close( lockfd ); lockfd = -1;
	}
	if( Server_names_DYN ){
		Free_line_list(&info);
		Split(&info, Server_names_DYN, File_sep, 0,0,0,0,0,0);
		for( ix = 0; ix < info.count; ++ix ){
			DEBUGF(DLPQ3)("Get_queue_status: getting subserver status '%s'", 
				info.list[ix] );
			Set_DYN(&Printer_DYN,info.list[ix]);
			Get_local_or_remote_status( tokens, sock, displayformat,
				status_lines, done_list, max_size, hash_key );
			DEBUGF(DLPQ3)("Get_queue_status: finished subserver status '%s'", 
				info.list[ix] );
		}
	} else if( Destinations_DYN ){
		Free_line_list(&info);
		Split(&info, Destinations_DYN, File_sep, 0,0,0,0,0,0);
		for( ix = 0; ix < info.count; ++ix ){
			DEBUGF(DLPQ3)("Get_queue_status: getting destination status '%s'", 
				info.list[ix] );
			Set_DYN(&Printer_DYN,info.list[ix]);
			Get_local_or_remote_status( tokens, sock, displayformat,
				status_lines, done_list, max_size, hash_key );
			DEBUGF(DLPQ3)("Get_queue_status: finished destination status '%s'", 
				info.list[ix] );
		}
	} else if( RemoteHost_DYN ){
		/* now we look at the remote host */
		if( Find_fqdn( &LookupHost_IP, RemoteHost_DYN )
			&& ( !Same_host(&LookupHost_IP,&Host_IP )
				|| !Same_host(&LookupHost_IP,&Localhost_IP )) ){
			DEBUGF(DLPQ1)("Get_queue_status: doing local");
			if( safestrcmp(RemotePrinter_DYN, Printer_DYN) ){
				Set_DYN(&Printer_DYN,RemotePrinter_DYN);
				Get_queue_status( tokens, sock, displayformat, status_lines,
					done_list, max_size, hash_key );
			} else {
				SNPRINTF(msg,sizeof(msg))"Error: loop in printcap- %s@%s -> %s@%s\n",
					Printer_DYN, FQDNHost_FQDN, RemotePrinter_DYN, RemoteHost_DYN );
				Write_fd_str(*sock, msg );
			}
		} else {
			DEBUGF(DLPQ1)("Get_queue_status: doing remote %s@%s",
				RemotePrinter_DYN, RemoteHost_DYN);
			if( Remote_support_DYN ) uppercase( Remote_support_DYN );
			if( safestrchr( Remote_support_DYN, 'Q' ) ){
#ifdef ORIGINAL_DEBUG//JY@1020
				fd = Send_request( 'Q', displayformat, tokens->list, Connect_timeout_DYN,
					Send_query_rw_timeout_DYN, *sock );
#endif
				if( fd >= 0 ){
					char *tempfile;
					/* shutdown( fd, 1 ); */
					tempfd = Make_temp_fd( &tempfile );
					while( (nx = read(fd,msg,sizeof(msg))) > 0 ){
						if( Write_fd_len(tempfd,msg,nx) < 0 ) cleanup(0);
					}
					close(fd); fd = -1;
					Print_different_last_status_lines( sock, tempfd, status_lines, 0 );
					close(tempfd); tempfd = -1;
					unlink( tempfile );
				}
			}
		}
	}

	DEBUGF(DLPQ3)("Get_queue_status: finished '%s'", Printer_DYN );
	goto done;

 error:
	SNPRINTF(header,sizeof(header))"Printer: %s@%s - ERROR: %s",
		Printer_DYN, Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN, error );
	DEBUGF(DLPQ1)("Get_queue_status: error msg '%s'", header );
	if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
 done:
	if( savedfd > 0 ) *sock = savedfd;
	Free_line_list(&info);
	Free_line_list(&lineinfo);
	Free_line_list(&outbuf);
	Free_line_list(&cache);
	Free_line_list(&cache_info);
	return;
}

void Print_status_info( int *sock, char *file,
	char *prefix, int status_lines, int max_size )
{
	char *image;
	static char *atmsg = " at ";
	struct line_list l;
	int start, i;
	Init_line_list(&l);

	DEBUGF(DLPQ1)("Print_status_info: '%s', lines %d, size %d",
		file, status_lines, max_size );
	if( status_lines > 0 ){
		i = (status_lines * 100)/1024;
		if( i == 0 ) i = 1;
		image = Get_file_image(file, i);
		Split(&l,image,Line_ends,0,0,0,0,0,0);
		if( l.count < status_lines ){
			if( image ) free( image ); image = 0;
			image = Get_file_image(file, 0);
			Split(&l,image,Line_ends,0,0,0,0,0,0);
		}
	} else {
		image = Get_file_image(file, max_size);
		Split(&l,image,Line_ends,0,0,0,0,0,0);
	}

	DEBUGF(DLPQ1)("Print_status_info: line count %d", l.count );

	start = 0;
	if( status_lines ){
		start = l.count - status_lines;	
		if( start < 0 ) start = 0;
	}
	for( i = start; i < l.count; ++i ){
		char *s, *t, *u;
		s = l.list[i];
		if( (t = strstr( s, " ## " )) ){
			*t = 0;
		}
		/* make the date format short */
		if( !Full_time_DYN ){
			for( u = s; (t = strstr(u,atmsg)); u = t+safestrlen(atmsg) );
			if( u != s && (t = strrchr( u, '-' )) ){
				memmove( u, t+1, safestrlen(t+1)+1 );
			}
		}
		if( prefix && Write_fd_str(*sock,prefix) < 0 ) cleanup(0);
		if( Write_fd_str(*sock,s) < 0 ) cleanup(0);
		if( Write_fd_str(*sock,"\n") < 0 ) cleanup(0);
	}
	Free_line_list(&l);
	if( image) free(image); image = 0;
}

void Print_different_last_status_lines( int *sock, int fd,
	int status_lines, int max_size )
{
	char header[SMALLBUFFER];
	struct line_list l;
	int start, last_printed, i, j, same;
	char *s, *t;

	Init_line_list(&l);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DLPQ1)("Print_different_last_status_lines: status lines %d", status_lines );
#endif
	Get_fd_image_and_split(fd,max_size,0,&l,Line_ends,0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPQ1)Dump_line_list( "Print_different_last_status_lines", &l );
#endif

	header[0] = 0;
	last_printed = start = -1;
	if( status_lines > 0 ) for( i = 0; i < l.count; ++i ){
		s = l.list[i];
		/* find up to the first colon */
		if( (t = safestrchr(s,':')) ){
			*t = 0;
		}
		same = !safestrcmp( header, s );
		if( !same ){
			safestrncpy(header,s);
		}
		if( t ) *t = ':';
		if( !same ){
			/* we print from i-1-(status_lines-1) to i-1 */
			start = i-status_lines;
			if( start <= last_printed ) start = last_printed + 1;
			for( j = start; j < i; ++j ){
				if( Write_fd_str(*sock,l.list[j]) < 0 ) cleanup(0);
				if( Write_fd_str(*sock,"\n") < 0 ) cleanup(0);
			}
			last_printed = i-1;
			DEBUGF(DLPQ1)("Print_different_last_status_lines: start %d, last_printed %d",
				start, last_printed );
		}
	}
	if( status_lines > 0 ){
		start = l.count - status_lines;
	}
	if( start <= last_printed ) start = last_printed + 1;
	DEBUGF(DLPQ1)("Print_different_last_status_lines: done, start %d", start );
	for( i = start; i < l.count ; ++i ){
		if( Write_fd_str(*sock,l.list[i]) < 0 ) cleanup(0);
		if( Write_fd_str(*sock,"\n") < 0 ) cleanup(0);
	}
	Free_line_list(&l);
}


void Get_local_or_remote_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size, char *hash_key )
{
	char msg[SMALLBUFFER];
	int fd, n, tempfd;

	/* we have to see if the host is on this machine */

	DEBUGF(DLPQ1)("Get_local_or_remote_status: %s", Printer_DYN );
	if( !safestrchr(Printer_DYN,'@') ){
		DEBUGF(DLPQ1)("Get_local_or_remote_status: doing local");
		Get_queue_status( tokens, sock, displayformat, status_lines,
			done_list, max_size, hash_key );
		return;
	}
	Fix_Rm_Rp_info(0,0);
	/* now we look at the remote host */
	if( Find_fqdn( &LookupHost_IP, RemoteHost_DYN )
		&& ( !Same_host(&LookupHost_IP,&Host_IP )
			|| !Same_host(&LookupHost_IP,&Localhost_IP )) ){
		DEBUGF(DLPQ1)("Get_local_or_remote_status: doing local");
		Get_queue_status( tokens, sock, displayformat, status_lines,
			done_list, max_size, hash_key );
		return;
	}
	uppercase( Remote_support_DYN );
	if( safestrchr( Remote_support_DYN, 'Q' ) ){
		DEBUGF(DLPQ1)("Get_local_or_remote_status: doing remote %s@%s",
			RemotePrinter_DYN, RemoteHost_DYN);
#ifdef ORIGINAL_DEBUG//JY@1020
		fd = Send_request( 'Q', displayformat, tokens->list, Connect_timeout_DYN,
			Send_query_rw_timeout_DYN, *sock );
#endif
		if( fd >= 0 ){
			/* shutdown( fd, 1 ); */
			tempfd = Make_temp_fd( 0 );
			while( (n = read(fd,msg,sizeof(msg))) > 0 ){
				if( Write_fd_len(tempfd,msg,n) < 0 ) cleanup(0);
			}
			close(fd); fd = -1;
			Print_different_last_status_lines( sock, tempfd, status_lines, 0 );
			close(tempfd);
		}
	}
}
#endif

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
"$Id: lpd_remove.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "lpd_remove.h"
#include "getqueue.h"
#include "getprinter.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "permission.h"
#include "child.h"
#include "proctitle.h"
#include "fileopen.h"
#include "sendreq.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Tue May  2 09:32:50 PDT 1995
 * 
 * Remove a Job.
 * This is very similar to the status program.
 * 
 * 1. We check for permissions first
 *    - first we check to see if the remote host has permissions
 *    - next we check to see if the user has permissions
 * Note:
 *   if we have control permissions,  then we can remove any job.
 *   Normally,  the options passed are 'user jobnumber' and we
 *   use the user name and/or job number to select them.  When we have
 *   control permissions,  we are not restricted to our own jobs.
 *     so: if we have control permissions,  AND pass an option, we
 *         do not check for our name.
 *         if we have control permissions AND we do not pass an option,
 *         we check for our name.
 *
 *  we have \006printer user key key key
 *              0       1    2 ...        index
 ***************************************************************************/

int Job_remove( int *sock, char *input )
{
printf("Job_remove\n");//JY
#if !defined(JYWENG20031106remove)
	if( input && *input ) ++input;//JY1114

	if(get_queue_name(input))
	{
		printf("QueueName is not LPRServer\n");
		send_ack_packet(sock, ACK_FAIL);//JY1120
		return(0);
	}
	else
		printf("QueueName is LPRServer\n");

	if(lptstatus.pid != 0){
		check_prn_status(ONLINE, "");
		kill(lptstatus.pid, SIGKILL);	
	}
	else
		return(0);
#endif

#ifdef REMOVE
	char error[LINEBUFFER];
	int i;
	char *name, *s, *user = 0;
	struct line_list tokens, done_list;

	Init_line_list(&tokens);
	Init_line_list(&done_list);
	Name = "Job_remove";

	/* get the options */
	++input;
	DEBUGF(DLPRM1)("Job_remove: input '%s'", input );
	Split(&tokens,input,Whitespace,0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPRM2)Dump_line_list("Job_remove: input", &tokens );
#endif

	/* check printername for characters, underscore, digits */

	if( tokens.count < 2 ){
		SNPRINTF( error, sizeof(error))
			_("missing user or printer name"));
		goto error;
	}
	name = tokens.list[0];

	DEBUGF(DLPRM1)("Job_remove: checking '%s'", name );
	if( (s = Is_clean_name( name )) ){
		SNPRINTF( error, sizeof(error))
			_("printer '%s' has illegal character at '%s' in name"), name, s );
		goto error;
	}
	DEBUGF(DLPRM1)("Job_remove: result '%s'", name );
	Set_DYN(&Printer_DYN,name);

	user = safestrdup(tokens.list[1],__FILE__,__LINE__);
	Perm_check.remoteuser = user;

	/* remove the first two tokens */
	Remove_line_list(&tokens,1);
	Remove_line_list(&tokens,0);
	Check_max(&tokens,1);
	tokens.list[tokens.count] = 0;

	if( safestrcmp( Printer_DYN, ALL ) ){
		DEBUGF(DLPRM2)( "Job_remove: checking printcap entry '%s'",  Printer_DYN );
		Set_DYN(&Printer_DYN, Printer_DYN );
		Get_queue_remove( user, sock, &tokens, &done_list );
	} else {
/*1103
		Get_all_printcap_entries();
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN, All_line_list.list[i]);
			Get_queue_remove( user, sock, &tokens, &done_list );
		}
*/
	}
	goto done;

 error:
	LOGMSG( LOG_INFO) _("Job_remove: error '%s'"), error );
	DEBUGF(DLPRM2)("Job_remove: error msg '%s'", error );
	safestrncat(error,"\n");
	if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
 done:
	DEBUGF(DLPRM2)( "Job_remove: done" );
	if( user ) free(user); user = 0;
	Free_line_list(&done_list);
	Free_line_list(&tokens);
#endif
	return( 0 );
}

#ifdef REMOVE
/***************************************************************************
 * void Get_queue_remove
 *  - find and remove the spool queue entries
 ***************************************************************************/

void Get_queue_remove( char *user, int *sock, struct line_list *tokens,
	struct line_list *done_list )
{
	char msg[SMALLBUFFER], header[SMALLBUFFER];
	int control_perm, permission, count, removed, status,
		i, c = 0, pid, fd;
	char *s, *identifier;
	struct stat statb;
	struct line_list info, active_pid;
	struct job job;

	Init_line_list(&info);
	Init_line_list(&active_pid);
	Init_job(&job);

	/* set printer name and printcap variables */

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPRM2)Dump_line_list("Get_queue_remove - tokens", tokens );
	DEBUGF(DLPRM2)( "Get_queue_remove: user '%s', printer '%s'",
		user, Printer_DYN );
#endif

	Errorcode = 0;

	setproctitle( "lpd LPRM '%s'", Printer_DYN );
	/* first check to see if you have control permissions */

	msg[0] = 0;
	status = Setup_printer( Printer_DYN, msg, sizeof(msg), 0 );
	if( status ){
		if( msg[0] == 0 ){
			DEBUGF(DLPRM2)("Get_queue_remove: cannot set up printer '%s'", Printer_DYN);
		}
		goto error;
	}

	c = Debug;
	i = DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if( !s ) s = New_debug_DYN;
	Parse_debug( s, 0 );

	if( !(DbgFlag & DLPRMMASK) ){
		Debug = c;
		DbgFlag = i;
	} else {
		i = Debug;
		Debug = c;
		if( Log_file_DYN ){
			fd = Trim_status_file( -1, Log_file_DYN, Max_log_file_size_DYN,
				Min_log_file_size_DYN );
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = i;
	}

	/* set up status */
	if( Find_exists_value(done_list,Printer_DYN,Value_sep ) ){
		return;
	}
	Add_line_list(done_list,Printer_DYN,Value_sep,1,1);

	/* check for permissions */

	Perm_check.service = 'C';
	Perm_check.printer = Printer_DYN;
	Perm_check.host = 0;
	Perm_check.user = 0;

	control_perm = Perms_check( &Perm_line_list, &Perm_check, 0, 0 );
	DEBUGF(DLPRM2)("Job_status: permission '%s'", perm_str(control_perm));

	if( control_perm != P_ACCEPT ) control_perm = 0;

	SNPRINTF( msg, sizeof(msg)) _("Printer %s@%s:\n"),
		Printer_DYN, ShortHost_FQDN );
	Write_fd_str( *sock, msg );

	Free_line_list( &Sort_order );
	Scan_queue( &Spool_control, &Sort_order,0,0,0,0,0,0,0,0 );
	DEBUGF(DLPRM2)("Get_queue_remove: total files %d", Sort_order.count );

	/* scan the files to see if there is one which matches */
	removed = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPRM3)Dump_line_list("Get_queue_remove - tokens", tokens );
#endif
	for( count = 0; count < Sort_order.count; ++count ){
		Free_job(&job);
		Get_hold_file(&job, Sort_order.list[count] );

#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DLPRM3)Dump_job("Get_queue_remove - info",&job);
#endif
        if( tokens->count && Patselect( tokens, &job.info, 0) ){
			continue;
        }

		/* get everything for the job now */
		Setup_cf_info( &job, 0 );
		identifier = Find_str_value(&job.info,IDENTIFIER,Value_sep);
		if( !identifier ) identifier
			= Find_str_value(&job.info,TRANSFERNAME,Value_sep);

		DEBUGF(DLPRM3)("Get_queue_remove: matched '%s'", identifier );
		SNPRINTF( msg, sizeof(msg)) _("  checking perms '%s'\n"),
			identifier );
		Write_fd_str( *sock, msg );


		/* we check to see if we can remove this one if we are the user */
		if( control_perm == 0 ){
			/* now we get the user name and IP address */
			Perm_check.user = Find_str_value(&job.info,LOGNAME,Value_sep);
			Perm_check.host = 0;
			if( (s = Find_str_value(&job.info,FROMHOST,Value_sep)) 
				&& Find_fqdn( &PermHost_IP, s ) ){
				Perm_check.host = &PermHost_IP;
			}
			Perm_check.service = 'M';
			permission = Perms_check( &Perm_line_list, &Perm_check, &job, 1 );
			if( permission == P_REJECT ){
				SNPRINTF( msg, sizeof(msg)) _("  no permissions '%s'\n"),
					identifier );
				Write_fd_str( *sock, msg );
				continue;
			}
		}

		/* log this to the world */
		DEBUGF(DLPRM4)("Get_queue_remove: removing '%s'", identifier );
		SNPRINTF( msg, sizeof(msg)) _("  dequeued '%s'\n"), identifier );
		Write_fd_str( *sock, msg );

#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( &job, "LPRM", "start" );
#endif
		if( Remove_job( &job ) ){
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, "LPRM", "fail" );
#endif
			SNPRINTF( msg, sizeof(msg))
				_("error: could not remove '%s'"), identifier ); 
			Write_fd_str( *sock, msg );
			goto error;
		}
#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( &job, "LPRM", "success" );
#endif
		if( (pid = Find_flag_value(&job.info,SERVER,Value_sep)) ){
			DEBUGF(DLPRM4)("Get_queue_remove: active_pid %d", pid );
			if( kill( pid, 0 ) == 0 ){
				Check_max(&active_pid,1);
				active_pid.list[active_pid.count++] = Cast_int_to_voidstar(pid);
			}
		}
		++removed;
		if( tokens->count == 0 ) break;
	}
	Free_line_list(&info);
	Free_job(&job);
	Free_line_list( &Sort_order );
	if( removed ){
		for( i = 0; i < active_pid.count; ++i ){
			pid = Cast_ptr_to_int(active_pid.list[i]);
			active_pid.list[i] = 0;
			DEBUGF(DLPRM2)("Get_queue_remove: killing pid '%d' SIGHUP/SIGINT/SIGQUIT/SIGCONT", pid );
			killpg( pid, SIGHUP );
			kill( pid, SIGHUP );
			killpg( pid, SIGINT );
			kill( pid, SIGINT );
			killpg( pid, SIGQUIT );
			kill( pid, SIGQUIT );
			killpg( pid, SIGCONT );
			kill( pid, SIGCONT );
		}
		/* kill spooler process */
		
		pid = 0;
#if 0//1103
		if( (fd = Checkread( Queue_lock_file_DYN, &statb )) >= 0 ){
			pid = Read_pid( fd, (char *)0, 0 );
			close( fd );
		}
#else
#endif
		DEBUGF(DLPRM2)("Get_queue_status: checking server pid %d", pid );
		/* kill active spooler */
		if( pid > 0 ){
			kill( pid, SIGUSR2 );
		}
	}

	if( Server_names_DYN ){
		Free_line_list(&info);
		Split(&info, Server_names_DYN, File_sep, 0,0,0,0,0,0);
		for( i = 0; i < info.count; ++i ){
			DEBUGF(DLPRM2)("Get_queue_status: getting subserver status '%s'", 
				info.list[i] );
			Set_DYN(&Printer_DYN,info.list[i]);
			Get_local_or_remote_remove( user, sock, tokens, done_list );
			DEBUGF(DLPRM2)("Get_queue_status: finished subserver status '%s'", 
				info.list[i] );
		}
	} else if( Destinations_DYN ){
		Free_line_list(&info);
		Split(&info, Destinations_DYN, File_sep, 0,0,0,0,0,0);
		for( i = 0; i < info.count; ++i ){
			DEBUGF(DLPRM2)("Get_queue_status: getting destination status '%s'", 
				info.list[i] );
			Set_DYN(&Printer_DYN,info.list[i]);
			Get_local_or_remote_remove( user, sock, tokens, done_list );
			DEBUGF(DLPRM2)("Get_queue_status: finished destination status '%s'", 
				info.list[i] );
		}
	} else if( RemoteHost_DYN ){
		if( Find_fqdn( &LookupHost_IP, RemoteHost_DYN )
			&& ( !Same_host(&LookupHost_IP,&Host_IP )
				|| !Same_host(&LookupHost_IP,&Localhost_IP )) ){
			DEBUGF(DLPQ1)("Get_local_or_remote_status: doing local");
			if( safestrcmp(RemotePrinter_DYN, Printer_DYN) ){
				Set_DYN(&Printer_DYN,RemotePrinter_DYN);
				Get_queue_remove( user, sock, tokens, done_list );
			} else {
				SNPRINTF(msg,sizeof(msg))"Error: loop in printcap- %s@%s -> %s@%s\n",
					Printer_DYN, FQDNHost_FQDN, RemotePrinter_DYN, RemoteHost_DYN );
				Write_fd_str(*sock, msg );
			}
		} else {
			/* put user name at start of list */
			Check_max(tokens,2);
			for( i = tokens->count; i > 0; --i ){
				tokens->list[i] = tokens->list[i-1];
			}
			tokens->list[0] = user;
			++tokens->count;
			tokens->list[tokens->count] = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
			fd = Send_request( 'M', REQ_REMOVE, tokens->list, Connect_timeout_DYN,
				Send_query_rw_timeout_DYN, *sock );
#endif
			if( fd >= 0 ){
				shutdown( fd, 1 );
				while( (c = read(fd,msg,sizeof(msg))) > 0 ){
					Write_fd_len(*sock,msg,c);
				}
				close(fd); fd = -1;
			}
			for( i = 0; i < tokens->count; ++i ){
				tokens->list[i] = tokens->list[i+1];
			}
			--tokens->count;
		}
	}

	DEBUGF(DLPRM2)("Get_queue_remove: finished '%s'", Printer_DYN );
	goto done;

 error:
	DEBUGF(DLPRM2)("Get_queue_remove: error msg '%s'", msg );
	SNPRINTF(header, sizeof(header)) "Printer: %s", Printer_DYN );
	safestrncpy( header, _(" ERROR: ") );
	safestrncat( header, msg );
	safestrncat( header, "\n" );
	Write_fd_str( *sock, header );
 done:
	active_pid.count = 0;
	Free_line_list(&info);
	Free_line_list(&active_pid);
	Free_job(&job);
	return;
}

void Get_local_or_remote_remove( char *user, int *sock,
	struct line_list *tokens, struct line_list *done_list )
{
	char msg[LARGEBUFFER];
	int fd, n, i;

	/* we have to see if the host is on this machine */

	if( !safestrchr(Printer_DYN,'@') ){
		Get_queue_remove( user, sock, tokens, done_list );
		return;
	}
/*1103
	Fix_Rm_Rp_info(0,0);
*/
	/* now we look at the remote host */
	if( Find_fqdn( &LookupHost_IP, RemoteHost_DYN )
		&& ( !Same_host(&LookupHost_IP,&Host_IP )
			|| !Same_host(&LookupHost_IP,&Localhost_IP )) ){
		Get_queue_remove( user, sock, tokens, done_list );
		return;
	}
	/* put user name at start of list */
	Check_max(tokens,2);
	for( i = tokens->count; i > 0; --i ){
		tokens->list[i] = tokens->list[i-1];
	}
	tokens->list[0] = user;
	++tokens->count;
	tokens->list[tokens->count] = 0;
#ifdef ORIGINAL_DEBUG//JY@1020
	fd = Send_request( 'M', REQ_REMOVE, tokens->list, Connect_timeout_DYN,
		Send_query_rw_timeout_DYN, *sock );
#endif
	if( fd >= 0 ){
		shutdown( fd, 1 );
		while( (n = read(fd,msg,sizeof(msg))) > 0 ){
			Write_fd_len(*sock,msg,n);
		}
		close(fd); fd = -1;
	}
	for( i = 0; i < tokens->count; ++i ){
		tokens->list[i] = tokens->list[i+1];
	}
	--tokens->count;
}

int Remove_file( char *openname )
{
	int fail = 0;
	struct stat statb;

	if( openname && stat( openname, &statb ) == 0 ){
		DEBUGF(DLPRM3)("Remove_file: removing '%s'", openname );
		if( unlink( openname ) || stat( openname, &statb ) == 0 ){
			LOGERR(LOG_INFO) "Remove_file: unlink did not remove '%s'",
				openname);
			fail |= 1;
		}
	}
	return( fail );
}

int Remove_job( struct job *job )
{
	int i;
	int fail = 0;
	char *identifier, *openname;
	struct line_list *datafile;

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DLPRM1)Dump_job("Remove_job",job);
#endif
#ifdef ORIGINAL_DEBUG//JY@1020
	setmessage(job,STATE,"REMOVE");
#endif
	identifier = Find_str_value(&job->info,IDENTIFIER,Value_sep);
#ifdef ORIGINAL_DEBUG//JY@1020
	setmessage( job, TRACE, "remove START" );
#endif
	if( !identifier ){
		identifier = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	}

	DEBUGF(DLPRM1)("Remove_job: identifier '%s'",identifier);
	fail = 0;
	for( i = 0; i < job->datafiles.count; ++i ){
		datafile = (void *)job->datafiles.list[i];
		openname = Find_str_value(datafile,OPENNAME,Value_sep);
		fail |= Remove_file( openname );
		openname = Find_str_value(datafile,TRANSFERNAME,Value_sep);
		fail |= Remove_file( openname );
	}
	openname = Find_str_value(&job->info,OPENNAME,Value_sep);
	fail |= Remove_file( openname );
	openname = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	fail |= Remove_file( openname );
	openname = Find_str_value(&job->info,HF_NAME,Value_sep);
	fail |= Remove_file( openname );

	if( fail == 0 ){
#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( job, TRACE, "remove SUCCESS" );
#endif
	} else {
#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( job, TRACE, "remove FAILED" );
#endif
	}
	if( Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN);
	}
	return( fail );
}
#endif



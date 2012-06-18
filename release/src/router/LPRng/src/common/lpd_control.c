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
"$Id: lpd_control.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "getopt.h"
#include "proctitle.h"
#include "control.h"
#include "child.h"
#include "getprinter.h"
#include "getqueue.h"
#include "fileopen.h"
#include "globmatch.h"
#include "permission.h"
#include "gethostinfo.h"
#include "lpd_control.h"

/**** ENDINCLUDE ****/

/***************************************************************************

 The control (LPC) interface provides the following functionality.

 1. Spool Queues have a 'control.printer' file that is read/written by
   the Get _spool_control and Set _spool_control routines.  These routines
   will happily put out the various control strings you need.
	USED BY: start/stop, enable/disable, debug, forward, holdall

 2. Individual jobs have a 'hold file' that is read/written by
   the Get_ job_control and Set_ job_control routines.  These also
   will read/write various control strings.
   USED by topq, hold, release

 ***************************************************************************/

 static char status_header[] = "%-18s %8s %8s %4s %7s %7s %8s %s%s";

int Job_control( int *sock, char *input )
{
#ifdef REMOVE
	struct line_list tokens;
	char error[LINEBUFFER];
	int tokencount;
	int i, action, permission;
	char *name, *user = 0, *s, *lpc_command;

	/* get the format */

	Init_line_list(&tokens);
	error[0] = 0;
	Name = "Job_control";
	++input;
	if( (s = safestrchr(input, '\n' )) ) *s = 0;
	DEBUGF(DCTRL1)("Job_control: socket %d, doing '%s'", *sock, input );

	/* check printername for characters, underscore, digits */
	Split(&tokens,input,Whitespace,0,0,0,0,0,0);
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DCTRL2)Dump_line_list("Job_control - input", &tokens);
#endif

	tokencount = tokens.count;
	if( tokencount < 3 ){
		SNPRINTF( error, sizeof(error))
			_("bad control command '%s'"), input );
		goto error;
	}


	/* get the name for the printer */
	/* it is either the default or user specified */

	name = tokens.list[0];
	if( tokencount > 3 ){
		name = tokens.list[3];
	}

	if( (s = Is_clean_name( name )) ){
		SNPRINTF( error, sizeof(error))
			_("printer '%s' has illegal char at '%s' in name"), name, s );
		goto error;
	}

	Set_DYN(&Printer_DYN,name);
	setproctitle( "lpd %s %s", Name, Printer_DYN );
	user = tokens.list[1];

	lpc_command = tokens.list[2];
	action = Get_controlword( lpc_command );
	if( action == 0 ){
		SNPRINTF( error, sizeof(error))
			_("%s: unknown control request '%s'"), Printer_DYN, lpc_command );
		goto error;
	}

	/* check the permissions for the action */

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list, "");
	}

	DEBUGF(DCTRL1)( "Job_control: checking USER='%s' SERVICE='%c', PRINTER='%s'",
		user, Perm_check.service, Printer_DYN );
	Perm_check.printer = Printer_DYN;
	Perm_check.remoteuser = user;
	Perm_check.service = 'C';
	Perm_check.lpc = lpc_command;
	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 ); /* queue perm check */
	DEBUGF(DCTRL1)( "Job_control: checked for '%c', permission %s",
		Perm_check.service, perm_str(permission) );

	switch( action ){
		case OP_REREAD:
			if( permission == P_REJECT ){ goto noperm; }
			DEBUGF(DCTRL1)( "Job_control: sending pid %d SIGHUP", Server_pid );
			SNPRINTF( error, sizeof(error)) "lpd server pid %d on %s, sending SIGHUP\n",
				Server_pid, FQDNHost_FQDN );
			(void)kill(Server_pid,SIGHUP);
			if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
			goto done;

		case OP_LPD:
			if( permission == P_REJECT ){ goto noperm; }
			DEBUGF(DCTRL1)( "Job_control: lpd pid %d", Server_pid );
			SNPRINTF( error, sizeof(error)) "lpd server pid %d on %s\n",
				Server_pid, FQDNHost_FQDN );
			if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
			error[0] = 0;
			goto done;

		case OP_DEFAULTQ:
			if( permission == P_REJECT ){ goto noperm; }
			Do_control_defaultq( sock );
			goto done;
		case OP_STATUS:
			/* we put out a space at the start to make PCNFSD happy */
			if( permission == P_REJECT ){ goto noperm; }
			SNPRINTF( error, sizeof(error)) status_header,
				" Printer", "Printing", "Spooling", "Jobs",
				"Server", "Subserver", "Redirect", "Status/(Debug)","" );
			safestrncat(error,"\n");
			if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
		case OP_STOP:
		case OP_START:
		case OP_DISABLE:
		case OP_ENABLE:
		case OP_ABORT:
		case OP_UP:
		case OP_DOWN:
		case OP_HOLDALL:
		case OP_NOHOLDALL:
			/* control line is 'Xprinter user action arg1 arg2
             *                    t[0]   t[1]  t[2]  t[3]
			 */
			if( tokencount > 4 ){
				/* we have a list of printers to use */
				for( i = 3; i < tokencount; ++i ){
					Name = "Job_control";
					Set_DYN(&Printer_DYN,tokens.list[i]);
					DEBUGF(DCTRL1)( "Job_control: doing printer '%s'", Printer_DYN );
					Do_printer_work( user, action, sock,
						0, error, sizeof(error) );
				}
				goto done;
			}
			break;
		case OP_MOVE:
			/* we have Nprinter user move jobid* target */
			if( tokencount < 5 ){
				SNPRINTF( error, sizeof(error))
					_("Use: OP_MOVE printer (user|jobid)* target") );
				goto error;
			}
			break;
	}
	Do_printer_work( user, action, sock,
		&tokens, error, sizeof(error) );
	goto done;

 noperm:
	SNPRINTF( error, sizeof(error))
		_("no permission to control server") );
 error:
	Name = "Job_control";
	DEBUGF(DCTRL2)("Job_control: error msg '%s'", error );
	safestrncat(error,"\n");
	Write_fd_str( *sock, error );
 done:
	Name = "Job_control";
	DEBUGF(DCTRL3)( "Job_control: DONE" );
	Free_line_list(&tokens);
#endif
	return(0);
}

void Do_printer_work( char *user, int action, int *sock,
	struct line_list *tokens, char *error, int errorlen )
{
#ifdef REMOVE
	int i;

	DEBUGF(DCTRL3)("Do_printer_work: printer '%s', action '%s'",
		Printer_DYN, Get_controlstr(action) );
	Name = "Do_printer_work";
	if( safestrcasecmp( Printer_DYN, ALL ) ){
		DEBUGF(DCTRL3)( "Do_printer_work: checking printcap entry '%s'",  Printer_DYN );
		Do_queue_control( user, action, sock,
			tokens, error, errorlen );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		Get_all_printcap_entries();
		for( i = 0; i < All_line_list.count; ++i ){
			Name = "Do_printer_work";
			Set_DYN(&Printer_DYN, All_line_list.list[i]);
			DEBUGF(DCTRL4)("Do_printer_work: printer [%d]='%s'", i, Printer_DYN );
			Do_queue_control( user, action, sock,
				tokens, error, errorlen);
			Name = "Do_printer_work";
		}
	}
#endif
}

/***************************************************************************
 * Do_queue_control()
 * do the actual queue control operations
 * - start, stop, enable, disable are simple
 * - others are more complex, and are handled in Do_control_file
 * We have tokens:
 *   printer user printer p1 p2 p3 -> p1 p2 p3
 ***************************************************************************/

void Do_queue_control( char *user, int action, int *sock,
	struct line_list *tokens, char *error, int errorlen )
{
#ifdef REMOVE
	char *start, *end;
	pid_t serverpid;			/* server pid to kill off */
	struct stat statb;			/* status of file */
	int fd, c, i, permission, db, dbflag;	/* descriptor and chars */
	char line[LINEBUFFER];
	char msg[LINEBUFFER];
	int status, change;
	int signal_server = SIGUSR1;
	struct line_list l;
	char *Action = "updated";
	/* first get the printer name */

	Init_line_list(&l);
	Name = "Do_queue_control";
	error[0] = 0;
	change = 0;

	if( action != OP_STATUS ){
		SNPRINTF(msg,sizeof(msg))"Printer: %s@%s\n",
			Printer_DYN,ShortHost_FQDN);
		Write_fd_str(*sock,msg);
	}
	if( Setup_printer( Printer_DYN, error, errorlen, 0 ) ){
		goto error;
	}

	c = Debug;
	i = DbgFlag;
	end = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if( !end ) end = New_debug_DYN;
	Parse_debug( end, 0 );

	if( !(DbgFlag & DCTRLMASK) ){
		Debug = c;
		DbgFlag = i;
	} else {
		db = Debug;
		dbflag = DbgFlag;
		Debug = c;
		DbgFlag = i;
		if( Log_file_DYN ){
			fd = Trim_status_file( -1, Log_file_DYN, Max_log_file_size_DYN,
				Min_log_file_size_DYN );
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = db;
		DbgFlag = dbflag;
	}

	Perm_check.printer = Printer_DYN;
	Perm_check.remoteuser = user;
	Perm_check.service = 'C';
	Perm_check.user = 0;
	Perm_check.host = 0;

	DEBUGF(DCTRL1)( "Do_queue_control: checking USER='%s' SERVICE='%c', PRINTER='%s'",
		user, Perm_check.service, Printer_DYN );

	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 ); /* queue check */
	DEBUGF(DCTRL1)( "Do_queue_control: C permission %s", perm_str(permission) );

	/*
	 * some of the commands allow a list of printers to be
	 * specified, others only take a single printer
	 * We need to put in the list stuff for the ones that take a list
	 */
	if( permission == P_REJECT ) goto noperm;

	switch( action ){
	case OP_LPQ:
		if( Do_control_lpq( user, action,
			tokens ) ){
			goto error;
		}
		goto done;
	case OP_PRINTCAP:
		Do_control_printcap( sock );
		goto done;
	case OP_STATUS:
		if( Do_control_status( sock,
			error, errorlen ) ){
			goto error;
		}
		goto done;
	case OP_STOP:
		Set_flag_value(&Spool_control,PRINTING_DISABLED, 1);
		break;
	case OP_START:
		Set_flag_value(&Spool_control,PRINTING_DISABLED, 0);
		Set_flag_value(&Spool_control,PRINTING_ABORTED, 0);
		break;
	case OP_DISABLE: Set_flag_value(&Spool_control,SPOOLING_DISABLED, 1); break;
	case OP_ENABLE: Set_flag_value(&Spool_control,SPOOLING_DISABLED, 0); break;
	case OP_ABORT:
		Set_flag_value(&Spool_control,PRINTING_ABORTED, 1);
	case OP_KILL:
		signal_server = SIGINT;
		break;
	case OP_UP:
		Set_flag_value(&Spool_control,PRINTING_ABORTED, 0);
		Set_flag_value(&Spool_control,PRINTING_DISABLED, 0);
		Set_flag_value(&Spool_control,SPOOLING_DISABLED, 0);
		break;
	case OP_DOWN:
		Set_flag_value(&Spool_control,PRINTING_DISABLED, 1);
		Set_flag_value(&Spool_control,SPOOLING_DISABLED, 1);
		break;
	case OP_HOLDALL:
		Set_flag_value(&Spool_control,HOLD_ALL, 1);
		break;
	case OP_NOHOLDALL:
		Set_flag_value(&Spool_control,HOLD_ALL, 0);
		break;
	case OP_RELEASE: case OP_REDO:
	case OP_TOPQ:
		Set_flag_value(&Spool_control,PRINTING_ABORTED, 0);
		Set_flag_value(&Spool_control,PRINTING_DISABLED, 0);
	case OP_HOLD:
		if( Do_control_file( action, sock,
			tokens, error, errorlen, 0 ) ){
			goto error;
		}
		break;
	case OP_MSG:
		Remove_line_list(tokens,0); /* pr */
		Remove_line_list(tokens,0); /* user */
		Remove_line_list(tokens,0); /* printer */
		Remove_line_list(tokens,0); /* 'msg' */
		start = Join_line_list(tokens," ");
		if( start ){
			end = start+safestrlen(start)-1;
			*end = 0;
		}
		DEBUGF(DCTRL1)("Do_queue_control: msg '%s'", start );
		if( Lpq_status_file_DYN ) unlink(Lpq_status_file_DYN );
		Set_str_value(&Spool_control,MSG,start);
		break;

	case OP_MOVE:
		--tokens->count;
		start = tokens->list[tokens->count];
		status = Do_control_file( action, sock,
			tokens, error, errorlen, start );
		++tokens->count;
		if( status ) goto error;
		break;

	case OP_LPRM:
		if( Do_control_lpq( user, action,
			tokens ) ){
			goto error;
		}
		break;
		
	case OP_REDIRECT:
		if( Do_control_redirect( sock,
			tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case OP_CLASS:
		if( Do_control_class( sock,
			tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case OP_DEBUG:
		if( Do_control_debug( sock,
			tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case OP_FLUSH:
		if( Lpq_status_file_DYN ) unlink(Lpq_status_file_DYN );
		{
			char *file;
			int fd = -1;
			if( (file = Queue_status_file_DYN) &&
				(fd = Checkwrite( file, &statb,O_RDWR,0,0)) > 0 ){
				ftruncate( fd, 0);
				close(fd);
			}
			if( (file = Status_file_DYN) &&
				(fd = Checkwrite( file, &statb,O_RDWR,0,0)) > 0 ){
				ftruncate( fd, 0);
				close(fd);
			}
		}
		signal_server = 0;
		break;
		
	default:
		SNPRINTF( error, errorlen) _("not implemented yet") );
		goto error;
	}

	Perm_check_to_list(&l, &Perm_check );
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DCTRL2)Dump_line_list("Do_queue_control - perms", &l);
#endif
	if( Server_queue_name_DYN ){
		Set_flag_value(&Spool_control,CHANGE,1);
	}
	/* modify the control file to force rescan of queue */
	Set_spool_control( &l, Queue_control_file_DYN, &Spool_control );
	Free_line_list(&l);

	/* signal or kill off the server */

	serverpid = 0;
	if( signal_server && (fd = Checkread( Queue_lock_file_DYN, &statb ) ) >= 0 ){
		serverpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
		if( serverpid ){
			if( signal_server != SIGUSR1 ){
				killpg( serverpid, signal_server );
				killpg( serverpid, SIGHUP );
				killpg( serverpid, SIGQUIT );
				kill( serverpid, signal_server );
				kill( serverpid, SIGHUP );
				kill( serverpid, SIGQUIT );
			}
			if( kill( serverpid, signal_server ) ){
				SNPRINTF(msg,sizeof(msg))_("server process PID %d exited\n"),
					serverpid );
			} else {
				SNPRINTF(msg,sizeof(msg))_("kill server process PID %d with %s\n"),
					serverpid, Sigstr(signal_server) );
			}
			Write_fd_str(*sock,msg);
		}
	}

	switch( action ){
	case OP_STATUS:	Action = 0; break; /* no message */
	case OP_UP:		Action = _("enabled and started"); break;
	case OP_DOWN:		Action = _("disabled and stopped"); break;
	case OP_STOP:		Action = _("stopped"); break;
	case OP_RELEASE: case OP_REDO: case OP_TOPQ:
	case OP_START:		Action = _("started"); break;
	case OP_DISABLE:	Action = _("disabled"); break;
	case OP_ENABLE:	Action = _("enabled"); break;
	case OP_REDIRECT:	Action = _("redirected"); break;
	case OP_HOLDALL:	Action = _("holdall on"); break;
	case OP_NOHOLDALL:	Action = _("holdall off"); break;
	case OP_MOVE:		Action = _("move done"); break;
	case OP_CLASS:		Action = _("class updated"); break;
	case OP_KILL:      Action = _("killed job"); break;
	case OP_ABORT:		Action = _("aborted job"); break;
	case OP_FLUSH:		Action = _("flushed status"); break;
	}
	if( Action ){
#ifdef ORIGINAL_DEBUG//JY@1020
		setmessage( 0, ACTION, "%s", Action );
#endif
		SNPRINTF( line, sizeof(line)) "%s@%s: %s\n",
			Printer_DYN, FQDNHost_FQDN, Action );
		if( Write_fd_str( *sock, line ) < 0 ) cleanup(0);
	}

	if( Server_queue_name_DYN ){
		if( Setup_printer( Server_queue_name_DYN, error, errorlen, 0 ) ){
			goto error;
		}
		serverpid = 0;
		if( signal_server && (fd = Checkread( Queue_lock_file_DYN, &statb ) ) >= 0 ){
			serverpid = Read_pid( fd, (char *)0, 0 );
			close( fd );
			if( serverpid == 0 || kill( serverpid, signal_server ) ){
				serverpid = 0;
			} else {
				SNPRINTF(msg,sizeof(msg))_(
				"WARNING: the main load balance server may have exited before\n"
				"it could be informed that there were new jobs.\n"
                "Please use 'lpc start %s' to start the server\n"),
					Server_queue_name_DYN );
				Write_fd_str(*sock,msg);
			}
		}
	}

	DEBUGF(DCTRL4)("Do_queue_control: server pid %d", serverpid );

	/* start the server if necessary */
	switch( action ){
	case OP_KILL:
			plp_sleep(1);
			serverpid = 0;
			break;
	case OP_START:
			serverpid = 0;
			break;
	}
	if( serverpid == 0 )switch( action ){
	case OP_KILL:
	case OP_TOPQ:
	case OP_RELEASE:
	case OP_REDO:
	case OP_UP:
	case OP_START:
	case OP_REDIRECT:
	case OP_MOVE:
	case OP_CLASS:
		SNPRINTF( line, sizeof(line)) "%s\n", Printer_DYN );
		DEBUGF(DCTRL3)("Do_queue_control: sending '%s' to LPD", line );
		if( Write_fd_str( Lpd_request, line ) < 0 ){
			LOGERR_DIE(LOG_ERR) _("Do_queue_control: write to fd '%d' failed"),
				Lpd_request );
		}
	}
	goto done;

 noperm:
	SNPRINTF( error, sizeof(error)) "no permission");
 error:
	DEBUGF(DCTRL2)("Do_queue_control: error msg '%s'", error );
	if( (i = safestrlen(error)) ){
		SNPRINTF( msg, sizeof(msg)) "%s@%s: %s\n",
			Printer_DYN, ShortHost_FQDN, error );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	}
 done:
	DEBUGF(DCTRL3)( "Do_queue_control: done" );
	Free_line_list(&l);
#endif
	return;
}


/***************************************************************************
 * Do_control_file:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. check to see if the control file has been selected
 * 3. update the hold file for the control file
 ***************************************************************************/

int Do_control_file( int action, int *sock,
	struct line_list *tokens, char *error, int errorlen, char *option )
{
	int i, permission, err;		/* ACME! Nothing but the best */
	int status, matchv;			/* status of last IO op */
	char msg[SMALLBUFFER];		/* message field */
	char *s, *identifier;
	struct job job;
	int destinations, update_dest;
	struct line_list l;

	/* get the job files */
	Init_line_list(&l);
	Init_job(&job);
	Free_line_list(&Sort_order);
#if defined(JYWENG20031104Scan_queue)
	if( Scan_queue( &Spool_control, &Sort_order,
			0,0,0,0,0,0,0,0) ){
		err = errno;
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF(error, errorlen)
			"Do_control_file: cannot read '%s' - '%s'",
			Spool_dir_DYN, Errormsg(err) );
#endif
			return(1);
	}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGF(DCTRL4)("Do_control_file: total files %d", Sort_order.count );
	DEBUGFC(DCTRL2)Dump_line_list("Do_control_file - tokens", tokens);
#endif

	/* scan the files to see if there is one which matches */

	status = 0;
	for( i = 0; status == 0 && i < Sort_order.count; ++i ){
		/*
		 * check to see if this entry matches any of the patterns
		 */
		Free_job(&job);
		Get_hold_file( &job, Sort_order.list[i] );
#ifdef ORIGINAL_DEBUG//JY@1020
		DEBUGFC(DCTRL2)Dump_job("Do_control_file - getting info",&job);
#endif
		identifier = Find_str_value(&job.info,IDENTIFIER,Value_sep);
		if( identifier == 0 ) identifier = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
		if( identifier == 0 ) continue;
		DEBUGF(DCTRL4)("Do_control_file: checking id '%s'", identifier );

		Perm_check.user = Find_str_value(&job.info,LOGNAME,Value_sep);
		Perm_check.host = 0;
		s = Find_str_value(&job.info,FROMHOST,Value_sep);
		if( s && Find_fqdn( &PermHost_IP, s ) ){
			Perm_check.host = &PermHost_IP;
		}
		permission = Perms_check( &Perm_line_list, &Perm_check, 0, 1 );
		DEBUGF(DCTRL1)( "Do_control_file: id '%s', user '%s', host '%s', permission %s",
			identifier, Perm_check.user, s, perm_str(permission) );
		if( permission == P_REJECT ){
			SNPRINTF( msg, sizeof(msg))
				_("%s: no permission '%s'\n"),
				Printer_DYN, identifier );
			if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
			continue;
		}

		destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep);

		update_dest = 0;

  next_dest:
		if( tokens->count > 4 ){
			/* you have printer user operation host key key key...
			 *           0      1    2         3    4
			 */
			for( matchv = Patselect( tokens, &job.info, 4 );
				matchv && update_dest < destinations; ++update_dest ){
				Get_destination(&job,update_dest);
				matchv = Patselect( tokens, &job.destination, 3 );
			}
			if( matchv ) continue;
		} else {
			status = 1;
		}

		DEBUGFC(DCTRL4){
			LOGDEBUG("Do_control_file: selected id '%s'", identifier );
			s = Find_str_value(&job.destination,IDENTIFIER,Value_sep);
			LOGDEBUG("Do_control_file: update_dest %d, id '%s'", update_dest, s );
		}
		
		/* we report this job being selected */
		switch( action ){
		case OP_HOLD:
			Set_flag_value(&job.info,HOLD_TIME,time((void *)0) );
			if( update_dest ){
				Set_flag_value(&job.destination,HOLD_TIME,time((void *)0) );
			}
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC held" );
#endif
			break;
		case OP_TOPQ:
			Set_flag_value(&job.info,HOLD_TIME,0 );
			Set_flag_value(&job.info,PRIORITY_TIME,time((void *)0) );
			if( update_dest ){
				Set_flag_value(&job.destination,HOLD_TIME,0 );
			}
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC topq");
#endif
			break;
		case OP_MOVE:
			Set_str_value(&job.info,MOVE,option);
			Set_flag_value(&job.info,HOLD_TIME,0 );
			Set_flag_value(&job.info,PRIORITY_TIME,0 );
			Set_flag_value(&job.info,DONE_TIME,0 );
			Set_flag_value(&job.info,REMOVE_TIME,0 );
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC move" );
#endif
			break;
		case OP_RELEASE:
			Set_flag_value(&job.info,HOLD_TIME,0 );
			Set_flag_value(&job.info,ATTEMPT,0 );
			if( update_dest ){
				Set_flag_value(&job.destination,HOLD_TIME,0 );
				Set_flag_value(&job.destination,ATTEMPT,0 );
			}
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC release" );
#endif
			break;
		case OP_REDO:
			Set_flag_value(&job.info,HOLD_TIME,0 );
			Set_flag_value(&job.info,ATTEMPT,0 );
			Set_flag_value(&job.info,DONE_TIME,0 );
			Set_flag_value(&job.info,REMOVE_TIME,0 );
			if( update_dest ){
				Set_flag_value(&job.destination,HOLD_TIME,0 );
				Set_flag_value(&job.destination,ATTEMPT,0 );
				Set_flag_value(&job.destination,DONE_TIME,0 );
				Set_flag_value(&job.destination,COPY_DONE,0 );
			}
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC redo");
#endif
			break;
		}
		if( update_dest ){
			Update_destination( &job );
		}
		SNPRINTF( msg, sizeof(msg)) _("%s: selected '%s'\n"),
			Printer_DYN, identifier );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
		Set_str_value(&job.info,ERROR,0 );
		Set_flag_value(&job.info,ERROR_TIME,0);
		/* record the last update person */
		Perm_check_to_list(&l, &Perm_check );
		if( Set_hold_file(&job,&l,0) ){
#ifdef ORIGINAL_DEBUG//JY@1020
			setmessage( &job, TRACE, "LPC failed" );
#endif
			SNPRINTF( msg, sizeof(msg))
				_("%s: cannot set hold file '%s'\n"),
				Printer_DYN, identifier );
			if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
		}
		Free_line_list(&l);
		if( update_dest ){
			goto next_dest;
		}
	}
	Free_job(&job);
	Free_line_list(&Sort_order);
	Free_line_list(&l);
	return( 0 );
}



/***************************************************************************
 * Do_control_lpq:
 *  forward an OP_LPQ or OP_LPRM
 ***************************************************************************/

int Do_control_lpq( char *user, int action,
	struct line_list *tokens )
{
#ifdef REMOVE
	char msg[LINEBUFFER];			/* message field */
	int i = 0;

	/* synthesize an OP_LPQ or OP_LPRM line */ 
	msg[sizeof(msg)-1] = 0;
	switch( action ){
	case OP_LPQ:  i = REQ_DSHORT; break;
	case OP_LPRM: i = REQ_REMOVE; break;
	}

	SNPRINTF( msg, sizeof(msg)) "%c%s", i, Printer_DYN );
	switch( action ){
		case OP_LPRM: 
			safestrncat( msg, " " );
			safestrncat( msg, user );
			break;
	}
	for( i = 0; i < tokens->count; ++i ){
		safestrncat( msg, " " );
		safestrncat( msg, tokens->list[i] );
	}
	safestrncat( msg, "\n" );
	DEBUGF(DCTRL3)("Do_control_lpq: sending '%s'", msg );

	/*
	switch( action ){
	case OP_LPQ: Job_status( sock,  msg ); break;
	case OP_LPRM: Job_remove( sock,  msg ); break;
	}
	*/
#endif
	return(0);
}

/***************************************************************************
 * Do_control_status:
 *  report current status
 ***************************************************************************/

int Do_control_status( int *sock,
	char *error, int errorlen )
{
#ifdef REMOVE
	char msg[SMALLBUFFER];			/* message field */
	char pr[LINEBUFFER];
	char pr_status[LINEBUFFER];
	char count[32];
	char server[32];
	char spooler[32];
	char forward[LINEBUFFER];
	char debugstr[LINEBUFFER];
	int serverpid, unspoolerpid;	/* server and unspooler */
	int len;
	char *s;
	int printable, held, move, err, done;

	/* get the job files */
	Free_line_list(&Spool_control);
	Get_spool_control( Queue_control_file_DYN, &Spool_control );
	if( Scan_queue( &Spool_control, &Sort_order, &printable,
			&held, &move,1,&err,&done,0,0) ){
#ifdef ORIGINAL_DEBUG//JY@1020
		SNPRINTF( error, errorlen)
			"Do_control_status: cannot read '%s' - '%s'",
			Spool_dir_DYN, Errormsg(errno) );
#endif
		return(1);
	}
	Free_line_list(&Sort_order);

	DEBUGF(DCTRL1)( "Do_control_status: printable %d, held %d, move %d, err %d, done %d",
		printable, held, move, error, done );

	/* now check to see if there is a server and unspooler process active */
	serverpid = Server_active( Printer_DYN );
	DEBUGF(DCTRL4)("Get_queue_status: serverpid %d", serverpid );

	unspoolerpid = Server_active( Queue_unspooler_file_DYN );
	DEBUGF(DCTRL4)("Get_queue_status: unspoolerpid %d", unspoolerpid );

	SNPRINTF( pr, sizeof(pr)) "%s@%s", Printer_DYN,
		Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
	pr_status[0] = 0;
	if( Hld_all(&Spool_control) ){
		len = safestrlen(pr_status);
		SNPRINTF( pr_status+len, sizeof(pr_status)-len) _(" holdall") );
	}
	if( (s = Clsses(&Spool_control)) ){
		len = safestrlen(pr_status);
		SNPRINTF( pr_status+len, sizeof(pr_status)-len) _(" class=%s"),s );
	}
	if( Auto_hold_DYN ){
		len = safestrlen(pr_status);
		SNPRINTF( pr_status+len, sizeof(pr_status)-len) _(" autohold") );
	}
	if( pr_status[0] ){
		len = safestrlen(pr_status);
		SNPRINTF( pr_status+len, sizeof(pr_status)-len) ")" );
		pr_status[0] = '(';
		
	}
	SNPRINTF( count, sizeof(count)) "%d", printable );
	strcpy( server, "none" );
	strcpy( spooler, "none" );
	if( serverpid ) SNPRINTF( server, sizeof(server))"%d",serverpid );
	if( unspoolerpid ) SNPRINTF( spooler, sizeof(spooler))"%d",unspoolerpid );
	if( Server_names_DYN ) SNPRINTF( spooler, sizeof(spooler))"%s",Server_names_DYN );

	forward[0] = 0;
	if( (s = Frwarding(&Spool_control)) ){
		SNPRINTF( forward, sizeof( forward )) "%s", s );
	}

	debugstr[0] = 0;
	if( (s = Cntrol_debug(&Spool_control)) ){
		SNPRINTF( debugstr, sizeof(debugstr)) "(%s)", s );
	}
	SNPRINTF( msg, sizeof(msg))
		status_header,
		pr,
		Pr_disabled(&Spool_control)?"disabled":(Pr_aborted(&Spool_control)?"aborted":"enabled"),
		Sp_disabled(&Spool_control)? "disabled" : "enabled",
		count, server, spooler, forward, pr_status, debugstr );
	trunc_str( msg );
	safestrncat(msg,"\n");
	if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
#endif
	return( 0 );
}


/***************************************************************************
 * Do_control_redirect:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report redirect name
 * 3. if option = none, remove redirect file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_redirect( int *sock,
	struct line_list *tokens, char *error, int errorlen )
{
	char *s;
	char msg[LINEBUFFER];
	int n = 0;

	/* get the spool entries */
#ifdef ORIGINAL_DEBUG//JY@1020
	DEBUGFC(DCTRL2)Dump_line_list("Do_control_redirect - tokens",tokens);
#endif
	switch( tokens->count ){
	case 3:
	case 4:
		n = 1;
		break;
	case 5:
		s = tokens->list[4];
		DEBUGF(DCTRL4)("Do_control_redirect: redirect to '%s'", s );
		if( safestrcasecmp( s, "off" ) == 0 ){
			Set_str_value(&Spool_control,FORWARDING,0);
		} else {
			Set_str_value(&Spool_control,FORWARDING,s);
		}
		break;

	default:
		SNPRINTF( error, errorlen)
			_("wrong number arguments, %d"), tokens->count );
		goto error;
	}

	s = Frwarding(&Spool_control);
	if( s ){
		SNPRINTF( msg, sizeof(msg)) _("forwarding to '%s'\n"), s );
	} else {
		SNPRINTF( msg, sizeof(msg)) _("forwarding off\n") );
	}

	if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);

 error:
	return( n );
}


/***************************************************************************
 * Do_control_class:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report class name
 * 3. if option = none, remove class file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_class( int *sock,
	struct line_list *tokens, char *error, int errorlen )
{
	char forward[LINEBUFFER];
	char *s;
	int n = 0;

	/* get the spool entries */

	error[0] = 0;
	forward[0] = 0;
	switch( tokens->count ){
	case -1:
	case 3: case 4:
		n = 1;
		break;
	case 5:
		s = tokens->list[4];
		DEBUGF(DCTRL4)("Do_control_class: class to '%s'", s );
		if( safestrcasecmp( s, "off" ) == 0 ){
			Set_str_value(&Spool_control,CLASS,0);
		} else {
			Set_str_value(&Spool_control,CLASS,s);
		}
		break;

	default:
		SNPRINTF( error, errorlen)
			_("wrong number arguments, %d"), tokens->count );
		goto error;
	}

	s = Find_str_value(&Spool_control,CLASS,Value_sep);

	if( s ){
		SNPRINTF( forward, sizeof(forward)) _("classes printed '%s'\n"),
			s );
	} else {
		SNPRINTF( forward, sizeof(forward)) _("all classes printed\n") );
	}

	if( Write_fd_str( *sock, forward ) < 0 ) cleanup(0);

 error:
	return( n );
}

/***************************************************************************
 * Do_control_debug:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report debug name
 * 3. if option = none, remove debug file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_debug( int *sock,
	struct line_list *tokens, char *error, int errorlen )
{
	char debugging[LINEBUFFER];
	char *s;
	int n = 0;

	/* get the spool entries */
	error[0] = 0;
	debugging[0] = 0;
	switch( tokens->count ){
	case -1:
	case 3: case 4:
		n = 1;
		break;
	case 5:
		s = tokens->list[4];
		DEBUGF(DCTRL4)("Do_control_debug: debug to '%s'", s );
		if( safestrcasecmp( s, "off" ) == 0 ){
			Set_str_value(&Spool_control,DEBUG,0);
		} else {
			Set_str_value(&Spool_control,DEBUG,s);
		}
		break;

	default:
		SNPRINTF( error, errorlen)
			_("wrong number arguments, %d"), tokens->count );
		goto error;
	}

	if( (s = Cntrol_debug(&Spool_control)) ){
		SNPRINTF( debugging, sizeof(debugging))
			_("debugging override set to '%s'"), s );
	} else {
		SNPRINTF( debugging, sizeof(debugging)) _("debugging override off") );
	}

	if( debugging[0] ){
		safestrncat(debugging,"\n");
		if( Write_fd_str( *sock, debugging ) < 0 ) cleanup(0);
	}

 error:
	return( n );
}


/***************************************************************************
 * Do_control_printcap:
 *  get the LPD status
 * 1. get the printcap PID
 * 2. if no options, report PID
 * 3. if option = HUP, send signal
 ***************************************************************************/

int Do_control_printcap( int *sock )
{
	char *printcap = 0, *s, *t, *w;

	/* get the spool entries */

	t = Join_line_list(&PC_alias_line_list,"|");
	s = Join_line_list(&PC_entry_line_list,"\n :");
	if( s && t ){
		if( (w = safestrrchr(t,'|')) ) *w = 0;
		printcap = safestrdup3(t,"\n :",s,__FILE__,__LINE__);
		if( (w = safestrrchr(printcap,' ')) ) *w = 0;
		if( Write_fd_str( *sock, printcap ) < 0 ) cleanup(0);
	} else {
		if( Write_fd_str( *sock, "\n" ) < 0 ) cleanup(0);
	}
	if( s ) free(s); s = 0;
	if( t ) free(t); t = 0;
	if( printcap ) free(printcap); printcap = 0;
	return(0);
}

#ifdef REMOVE
int Do_control_defaultq( int *sock )
{
	char msg [LINEBUFFER];

	Printer_DYN = 0;
	/* get the default queue */
	Get_printer();
	SNPRINTF( msg, sizeof(msg)) "%s\n", Printer_DYN );
	if ( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);

	return(0);
}
#endif

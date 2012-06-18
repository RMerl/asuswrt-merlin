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
"$Id: lpq.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpq.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lpq [ -PPrinter_DYN ]
 *    lpq [-Pprinter ]*[-a][-U username][-s][-l][+[n]][-Ddebugopt][job#][user]
 * DESCRIPTION
 *   lpq sends a status request to lpd(8)
 *   and reports the status of the
 *   specified jobs or all  jobs  associated  with  a  user.  lpq
 *   invoked  without  any arguments reports on the printer given
 *   by the default printer (see -P option).  For each  job  sub-
 *   mitted  (i.e.  invocation  of lpr(1)) lpq reports the user's
 *   name, current rank in the queue, the names of files compris-
 *   ing  the job, the job identifier (a number which may be sup-
 *   plied to lprm(1) for removing a specific job), and the total
 *   size  in  bytes.  Job ordering is dependent on the algorithm
 *   used to scan the spooling directory and is  FIFO  (First  in
 *   First Out), in order of priority level.  File names compris-
 *   ing a job may be unavailable (when lpr(1) is used as a  sink
 *   in  a  pipeline)  in  which  case  the  file is indicated as
 *   ``(STDIN)''.
 *    -P printer
 *         Specifies a particular printer, otherwise  the  default
 *         line printer is used (or the value of the PRINTER vari-
 *         able in the environment).  If PRINTER is  not  defined,
 *         then  the  first  entry in the /etc/printcap(5) file is
 *         reported.  Multiple printers can be displayed by speci-
 *         fying more than one -P option.
 *
 *   -a   All printers listed in the  /etc/printcap(5)  file  are
 *        reported.
 *
 *   -l   An alternate  display  format  is  used,  which  simply
 *        reports the user, jobnumber, and originating host.
 *
 *   [+[n]]
 *        Forces lpq to periodically display  the  spool  queues.
 *        Supplying  a  number immediately after the + sign indi-
 *        cates that lpq should sleep n seconds in between  scans
 *        of the queue.
 *        Note: the screen will be cleared at the start of each
 *        display using the 'curses.h' package.
 ****************************************************************************
 *
 * Implementation Notes
 * Patrick Powell Tue May  2 09:58:29 PDT 1995
 * 
 * The LPD server will be returning the formatted status;
 * The format can be the following:
 * 
 * SHORT:
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * Rank   Owner      Job  Files                                 Total Size
 * active root       30   standard input                        5 bytes
 * 2nd    root       31   standard input                        5 bytes
 * 
 * LONG:
 * 
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * 
 * root: 1st                                [job 030taco]
 *         standard input                   5 bytes
 * 
 * root: 2nd                                [job 031taco]
 *         standard input                   5 bytes
 * 
 */

#include "lp.h"

#include "child.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "sendreq.h"

/**** ENDINCLUDE ****/


#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpq.h"
/**** ENDINCLUDE ****/

 struct line_list Lpq_options;
 static char *Username_JOB;

#define MAX_SHORT_STATUS 6

/***************************************************************************
 * main()
 * - top level of LPQ
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	int i;
	struct line_list l, options;

	Init_line_list(&l);
	Init_line_list(&options);

	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) signal (SIGPIPE, SIG_IGN);
	(void) signal (SIGCHLD, SIG_DFL);


	/*
	 * set up the user state
	 */

#ifndef NODEBUG
	Debug = 0;
#endif

	Longformat = 1;
	Status_line_count = 0;
	Displayformat = REQ_DLONG;

	Initialize(argc, argv, envp, 'D' );
	Setup_configuration();
	Get_parms(argc, argv );      /* scan input args */
	if( Auth && !getenv("AUTH") ){
		FPRINTF(STDERR,_("authentication requested (-A) and no AUTH environment variable"));
		usage();
	}

	if(DEBUGL1)Dump_line_list("lpq- Config", &Config_line_list );
	/* we do the individual printers */
	if( Displayformat == REQ_DLONG && Longformat && Status_line_count <= 0 ){
		Status_line_count = (1 << (Longformat-1));
	}
	do {
		Free_line_list(&Printer_list);
		if( Clear_scr ){
			Term_clear();
			Write_fd_str(1,Time_str(0,0));
			Write_fd_str(1,"\n");
		}
		if( All_printers ){
			DEBUG1("lpq: all printers");
			Get_all_printcap_entries();
			if(DEBUGL1)Dump_line_list("lpq- All_line_list", &All_line_list );
			for( i = 0; i < All_line_list.count; ++i ){
				Set_DYN(&Printer_DYN,All_line_list.list[i] );
				Show_status(argv);
			}
		} else {
			/* set up configuration */
			Get_printer();
			Show_status(argv);
		}
		DEBUG1("lpq: done");
		Remove_tempfiles();
		DEBUG1("lpq: tempfiles removed");
		if( Interval > 0 ){
			plp_sleep( Interval );
		}
		/* we check to make sure that nobody killed the output */
	} while( Interval > 0 );
	DEBUG1("lpq: after loop");
	/* if( Clear_scr ){ Term_finish(); } */
	Errorcode = 0;
	DEBUG1("lpq: cleaning up");
	cleanup(0);
	return(0);
}

void Show_status(char **argv)
{
	int fd;
	char msg[LINEBUFFER];

	DEBUG1("Show_status: start");

	Fix_Rm_Rp_info(0,0);

	if( ISNULL(RemotePrinter_DYN) ){
		SNPRINTF( msg, sizeof(msg))
			_("Printer: %s - cannot get status from device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}

	if( Displayformat != REQ_DSHORT
		&& safestrcasecmp(Printer_DYN, RemotePrinter_DYN) ){
		SNPRINTF( msg, sizeof(msg)) _("Printer: %s is %s@%s\n"),
			Printer_DYN, RemotePrinter_DYN, RemoteHost_DYN );
		DEBUG1("Show_status: '%s'",msg);
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
	}
	if( Check_for_rg_group( Logname_DYN ) ){
		SNPRINTF( msg, sizeof(msg))
			_("Printer: %s - cannot use printer, not in privileged group\n"),
			Printer_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	if( Direct_DYN && Lp_device_DYN ){
		SNPRINTF( msg, sizeof(msg))
			_("Printer: %s - direct connection to device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	if( Auth ){
		Set_DYN(&Auth_DYN, getenv("AUTH") );
	}
	fd = Send_request( 'Q', Displayformat,
		&argv[Optind], Connect_timeout_DYN,
		Send_query_rw_timeout_DYN, 1 );
	if( fd >= 0 ){
		/* shutdown( fd, 1 ); */
		if( Read_status_info( RemoteHost_DYN, fd,
			1, Send_query_rw_timeout_DYN, Displayformat,
			Status_line_count ) ){
			cleanup(0);
		}
		close(fd); fd = -1;
	}
	DEBUG1("Show_status: end");
}


/***************************************************************************
 *int Read_status_info( int ack, int fd, int timeout );
 * ack = ack character from remote site
 * sock  = fd to read status from
 * char *host = host we are reading from
 * int output = output fd
 *  We read the input in blocks,  split up into lines,
 *  and then pass the lines to a lower level routine for processing.
 *  We run the status through the SNPRINTF() routine,  which will
 *   rip out any unprintable characters.  This will prevent magic escape
 *   string attacks by users putting codes in job names, etc.
 ***************************************************************************/

int Read_status_info( char *host, int sock,
	int output, int timeout, int displayformat,
	int status_line_count )
{
	int n, status, count, line, last_line, same;
	char header[SMALLBUFFER], buffer[SMALLBUFFER];
	char *s, *t;
	struct line_list l;
	int look_for_pr = 0;

	Init_line_list(&l);

	header[0] = 0;
	status = count = 0;
	/* long status - trim lines */
	DEBUG1("Read_status_info: output %d, timeout %d, dspfmt %d",
		output, timeout, displayformat );
	DEBUG1("Read_status_info: status_line_count %d", status_line_count );
	DEBUG1("Read_status_info: displayformat %d, Show_all %d",displayformat, Show_all );

	/*
	 * Do not try to be fancy - the overhead is not high unless lots
	 * of data and then something is wrong.
	 */
	if( displayformat == REQ_VERBOSE || displayformat == REQ_LPSTAT || Show_all ){
		do{ 
			if( (n = read( sock, buffer, sizeof(buffer)-1)) ){
				buffer[n] = 0;
				if( Write_fd_str( output, buffer ) < 0 ) return(1);
			}
		} while( n > 0 );
		return 0;
	}

	Read_fd_and_split( &l, sock, Line_ends, 0, 0, 0, 0, 0 );
	if(DEBUGL1)Dump_line_list("lpq- status", &l );
	last_line = -1;

	/* now deal with the short status format */
	if( displayformat == REQ_DSHORT ){
		for( line = 0; line < l.count; ++line ){
			s = l.list[line];
			if( s && !Find_exists_value(&Printer_list,s,0) ){
				if( Write_fd_str( output, s ) < 0
					|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				Add_line_list(&Printer_list,s,0,1,0);
			}
		}
		return(0);
	}
	
	same = 0;
	header[0] = 0;
	last_line = -1;
	look_for_pr = 1;
	for( line = 0; line < l.count; ){
		/* we start by looking at the first line and seeing if it is
		 * for a printer that we have already found
		 * if look_for_pr is 1 then we have just started the search
		 *    - we assume that any non-blank line not containing a Printer:
		 *      line is a 'proper' entry.
		 * if look_for_pr is 2 then we assume that we are looking for
		 *    the end of a printer entry and we look for a line with
		 *    Printer: in it that we have not seen 
		 */
		while( look_for_pr && line < l.count ){
			s = l.list[line];
			/* we do not want a line starting with a space or a blank line */
			if( ISNULL(s) ){
				look_for_pr = 1;
			} else if( look_for_pr == 1 &&
				!(strstr(s,"Printer:") || strstr(s,_("Printer:")) ) ){
				look_for_pr = 0;
			} else if( isspace(cval(s))
				/* if line starts with a space */
				/*  line does not contain Printer: */
				|| !(strstr(s,"Printer:") || strstr(s,_("Printer:")) )
				/* or that already is in the list */ 
				|| Find_exists_value(&Printer_list,s,0) ){
				look_for_pr = 2;
			} else {
				look_for_pr = 0;
			}
			if( look_for_pr == 0 ){
				if( Write_fd_str( output, s ) < 0
					|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				if( strstr(s,"Printer:") || strstr(s,_("Printer:")) ){
					Add_line_list(&Printer_list,s,0,1,0);
				}
				DEBUG1("Read_status_info: pr [%d] '%s'", line, s );
			}
			++line;
		}
		header[0] = 0;
		last_line = -1;
		while( !look_for_pr && line < l.count ){
			s = l.list[line];
			DEBUG1("Read_status_info: last_line %d, header '%s', checking [%d] '%s'",
				last_line, header, line, s );
			/* find up to the first colon */
			if( s == 0 ){
				++line;
				continue;
			}
			if( !Rawformat ){
				if( (t = safestrchr(s,':')) ){
					*t = 0;
				}
				same = 1;
				if( last_line == -1 ){
					last_line = line;
					safestrncpy( header, s );
					if( t ) *t = ':';
					++line;
					continue;
				}
				if( (same = isspace(cval(s))) ){
					same = !safestrcmp( header, s );
				}
				if( t ) *t = ':';
				if( same ){
					++line;
					continue;
				}
				DEBUG1("Read_status_info: header '%s', same %d", header, same );
				n = line - status_line_count;
				if( n < last_line ) n = last_line; 
				for( ; n < line; ++n ){
					t = l.list[n];
					if( Write_fd_str( output, t ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				}
				if( !isspace(cval(s)) &&
					(strstr(s,"Printer:") || strstr(s,_("Printer:")) ) ){
					look_for_pr = 1;
				}
				header[0] = 0;
				last_line = -1;
			} else {
				if( !isspace(cval(s)) &&
					(strstr(s,"Printer:") || strstr(s,_("Printer:")) ) ){
					look_for_pr = 1;
				} else {
					if( Write_fd_str( output, s ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					++line;
				}
			}
		}
	}
	DEBUG1("Read_status_info: after checks look_for_pr %d, line %d, last_line %d",
		look_for_pr, line, last_line);
	if( !look_for_pr && last_line >= 0 ){
		n = l.count - status_line_count;
		if( n < last_line ) n = last_line;
		for( ; n < l.count; ++n ){
			s = l.list[n];
			if( Write_fd_str( output, s ) < 0
				|| Write_fd_str( output, "\n" ) < 0 ) return(1);
		}
	}

	Free_line_list(&l);
	Free_line_list(&l);
	DEBUG1("Read_status_info: done" );
	return(0);
}

void Term_clear()
{
#if defined(CLEAR) 
	int pid, n;
	plp_status_t procstatus;
	if( (pid = dofork(0)) == 0 ){
		setuid( OriginalRUID );
		close_on_exec(3);
		execl(CLEAR,0);
		exit(1);
	} else if( pid < 0 ){
		LOGERR_DIE(LOG_ERR) _("fork() failed") );
	}
	while( (n = plp_waitpid(pid,&procstatus,0)) != pid ){
		int err = errno;
		DEBUG1("Filterprintcap: waitpid(%d) returned %d, err '%s'",
			pid, n, Errormsg(err) );
		if( err == EINTR ) continue; 
		LOGERR(LOG_ERR) _("Term_clear: waitpid(%d) failed"), pid);
		exit(1);
	}
#else
	Write_fd_str(1,"\014");
#endif
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 extern char *next_opt;

 char LPQ_optstr[]    /* LPQ options */
 = "AD:P:VacLn:lst:vU:" ;

void Get_parms(int argc, char *argv[] )
{
	int option;
	char *name, *s, *t;

	if( argv[0] && (name = safestrrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && safestrcmp( name, "lpstat" ) == 0 ){
		FPRINTF( STDERR,_("lpq:  please use the LPRng lpstat program\n"));
		exit(1);
	} else {
		/* scan the input arguments, setting up values */
		while ((option = Getopt (argc, argv, LPQ_optstr )) != EOF) {
			switch (option) {
			case 'A': Auth = 1; break;
			case 'D':
				Parse_debug(Optarg,1);
				break;
			case 'P': if( Optarg == 0 ) usage();
				Set_DYN(&Printer_DYN,Optarg);
				break;
			case 'V': ++Verbose; break;
			case 'a': Set_DYN(&Printer_DYN,ALL); All_printers = 1; break;
			case 'c': Clear_scr = 1; break;
			case 'l': ++Longformat; break;
			case 'n': Status_line_count = atoi( Optarg ); break;
			case 'L': Longformat = 0; Rawformat = 1; break;
			case 's': Longformat = 0;
						Displayformat = REQ_DSHORT;
						break;
			case 't': if( Optarg == 0 ) usage();
						Interval = atoi( Optarg );
						break;
			case 'v': Longformat = 0; Displayformat = REQ_VERBOSE; break;
			case 'U': Username_JOB = Optarg; break;
			default:
				usage();
			}
		}
	}
	if( Verbose ) {
		FPRINTF( STDOUT, "%s\n", Version );
		if( Verbose > 1 ){
			if( (s = getenv("LANG")) ){
				FPRINTF( STDOUT, _("LANG environment variable '%s'\n"), s );
				t = _("");
				if( t && *t ){
					FPRINTF( STDOUT, _("gettext translation information '%s'\n"), t );
				} else {
					FPRINTF( STDOUT, "%s", _("No translation available\n"));
				}
			} else {
				FPRINTF( STDOUT, "LANG environment variable not set\n" );
			}
			Printlist( Copyright, 2 );
		}
	}
}

 char *lpq_msg[] = {
 N_("usage: %s [-aAclV] [-Ddebuglevel] [-Pprinter] [-tsleeptime]\n"),
 N_("  -A           - use authentication specified by AUTH environment variable\n"),
 N_("  -a           - all printers\n"),
 N_("  -c           - clear screen before update\n"),
 N_("  -l           - increase (lengthen) detailed status information\n"),
 N_("                 additional l flags add more detail.\n"),
 N_("  -L           - maximum detailed status information\n"),
 N_("  -n linecount - linecount lines of detailed status information\n"),
 N_("  -Ddebuglevel - debug level\n"),
 N_("  -Pprinter    - specify printer\n"),
 N_("  -s           - short (summary) format\n"),
 N_("  -tsleeptime  - sleeptime between updates\n"),
 N_("  -V           - print version information\n"),
 N_("  -v           - print in key: value format\n"),
 0 };
 
void usage(void)
{
	char *s;
	int i;
	for( i = 0; (s = lpq_msg[i]); ++i ){
		if( i == 0 ){
			FPRINTF( STDERR, _(s), Name );
		} else {
			FPRINTF( STDERR, "%s", _(s) );
		}
	}
	Parse_debug("=",-1);
	FPRINTF( STDOUT, "%s\n", Version );
	exit(1);
}

#if 0

#include "permission.h"
#include "lpd_status.h"
/* int Send_request( */
	int class,					/* 'Q'= LPQ, 'C'= LPC, M = lprm */
	int format,					/* X for option */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	)
{
	int i, n;
	int socket = 1;
	char cmd[SMALLBUFFER];

	cmd[0] = format;
	cmd[1] = 0;
	SNPRINTF(cmd+1, sizeof(cmd)-1, "%s", RemotePrinter_DYN);
	for( i = 0; options[i]; ++i ){
		n = safestrlen(cmd);
		SNPRINTF(cmd+n,sizeof(cmd)-n," %s",options[i] );
	}
	Perm_check.remoteuser = "papowell";
	Perm_check.user = "papowell";
	Is_server = 1;
	Job_status(&socket,cmd);
	return(-1);
}
*/
#endif

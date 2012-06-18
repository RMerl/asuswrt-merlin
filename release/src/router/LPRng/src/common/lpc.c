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
"$Id: lpc.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


/***************************************************************************
 * SYNOPSIS
 *      lpc [ -PPrinter] [-S Server] [-U username ][-V] [-D debug] [command]
 * commands:
 *
 *   status [printer]     - show printer status (default is all printers)
 *   msg   printer  ....  - printer status message
 *   start [printer]      - start printing
 *   stop [printer]       - stop printing
 *   up [printer]         - start printing and spooling
 *   down [printer]       - stop printing and spooling
 *   enable [printer]     - enable spooling
 *   disable [printer]    - disable spooling
 *   abort [printer]      - stop printing, kill server
 *   kill [printer]       - stop printing, kill server, restart printer
 *   flush [printer]      - flush cached status
 *
 *   topq printer (user [@host] | host | jobnumer)*
 *   hold printer (all | user [@host] | host |  jobnumer)*
 *   release printer (all | user [@host] | host | jobnumer)*
 *
 *   lprm printer [ user [@host]  | host | jobnumber ] *
 *   lpq printer [ user [@host]  | host | jobnumber ] *
 *   lpd [pr | pr@host]   - PID of LPD server
 *   active [pr |pr@host] - check to see if server accepting connections
 *   client [all | pr ]     - show client configuration and printcap info 
 *   server [all |pr ]     - show server configuration and printcap info 
 *   defaultq              - show default queue for LPD server\n\
 *   defaults              - show default configuration values\n\
 *   lang                  - show current i18n language selection and support\n\
 *
 * DESCRIPTION
 *   lpc sends a  request to lpd(8)
 *   and reports the status of the command
 ****************************************************************************
 *
 * Implementation Notes
 * Patrick Powell Wed Jun 28 21:28:40 PDT 1995
 * 
 * The LPC program is an extremely simplified front end to the
 * LPC functionality in the server.  The commands send to the LPD
 * server have the following format:
 * 
 * \6printer user command options
 * 
 * If no printer is specified, the printer is the default from the
 * environment variable, etc.
 * 
 */

#include "lp.h"
#include "defs.h"
#include "initialize.h"
#include "getprinter.h"
#include "sendreq.h"
#include "child.h"
#include "control.h"
#include "getopt.h"
#include "patchlevel.h"
#include "errorcodes.h"

/**** ENDINCLUDE ****/


/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/


#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpc.h"

 void usage(void);
 void use_msg(void);
 void doaction( struct line_list *args );
 static char *Username_JOB;

int main(int argc, char *argv[], char *envp[])
{
	char *s;
	int i;
	char msg[ LINEBUFFER ];
	struct line_list args;

#if 0
	DEBUG1("%s",5);
	LOGDEBUG("%s",5);
	FATAL(LOGINFO)"%s",5);
	LOGERR(LOGINFO)"%s",5);
#endif
	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) signal( SIGPIPE, SIG_IGN );
	(void) signal( SIGCHLD, SIG_DFL);


	/*
	 * set up the user state
	 */
#ifndef NODEBUG
	Debug = 0;
#endif

	Init_line_list(&args);
	Initialize(argc, argv, envp, 'D' );
	Setup_configuration();

	/* scan the argument list for a 'Debug' value */

	Get_parms(argc, argv);      /* scan input args */
	if( Auth && !getenv( "AUTH" ) ){
		FPRINTF(STDERR,
		_("authentication requested (-A option) and AUTH environment variable not set") );
		usage();
	}

	DEBUG1("lpc: Printer '%s', Optind '%d', argc '%d'", Printer_DYN, Optind, argc );
	if(DEBUGL1){
		int ii;
		for( ii = Optind; ii < argc; ++ii ){
			LOGDEBUG( " [%d] '%s'", ii, argv[ii] );
		}
	}

	if( Username_JOB && OriginalRUID ){
		struct line_list user_list;
		char *str, *t;
		struct passwd *pw;
		int found;
		uid_t uid;

		DEBUG2("lpc: checking '%s' for -U perms",
			Allow_user_setting_DYN );
		Init_line_list(&user_list);
		Split( &user_list, Allow_user_setting_DYN,File_sep,0,0,0,0,0,0);
		
		found = 0;
		for( i = 0; !found && i < user_list.count; ++i ){
			str = user_list.list[i];
			DEBUG2("lpc: checking '%s'", str );
			uid = strtol( str, &t, 10 );
			if( str == t || *t ){
				/* try getpasswd */
				pw = getpwnam( str );
				if( pw ){
					uid = pw->pw_uid;
				}
			}
			DEBUG2( "lpc: uid '%d'", uid );
			found = ( uid == OriginalRUID );
			DEBUG2( "lpc: found '%d'", found );
		}
		if( !found ){
			DEBUG1( "%s", "-U (username) can only be used by ROOT" );
			Username_JOB = 0;
		}
	}
	if( Username_JOB ){
		Set_DYN(&Logname_DYN, Username_JOB);
	}

	if( Optind < argc ){
		for( i = Optind; argv[i]; ++i ){
			Add_line_list(&args,argv[i],0,0,0);
		}
		Check_max(&args,2);
		args.list[args.count] = 0;
		doaction( &args );
	} else while(1){
		FPRINTF( STDOUT, "lpc>" );
		if( fgets( msg, sizeof(msg), stdin ) == 0 ) break;
		if( (s = safestrchr( msg, '\n' )) ) *s = 0;
		DEBUG1("lpc: '%s'", msg );
		Free_line_list(&args);
		Split(&args,msg,Whitespace,0,0,0,0,0,0);
		Check_max(&args,2);
		args.list[args.count] = 0;
		if(DEBUGL1)Dump_line_list("lpc - args", &args );
		if( args.count == 0 ) continue;
		s = args.list[0];
		if(
			safestrcasecmp(s,"exit") == 0 || safestrcasecmp(s,_("exit")) == 0
			|| s[0] == 'q' || s[0] == 'Q' ){
			break;
		}
		doaction(&args);
	}
	Free_line_list(&args);
	Errorcode = 0;
	Is_server = 0;
	cleanup(0);
	return(0);
}

void doaction( struct line_list *args )
{
	int action, fd, n, argspos, pcinfo_header;
	struct line_list l;
	char msg[SMALLBUFFER];
	char *s, *t, *w, *printcap;

	Init_line_list(&l);
	s = t = w = printcap = 0;
	pcinfo_header = 0;
	if( args->count == 0 ) return;
	action = Get_controlword( args->list[0] );
	if(DEBUGL1)Dump_line_list("doaction - args", args );
	if( action == 0 ){
		use_msg();
		return;
	}
	if( args->count > 1 ){
		Set_DYN(&Printer_DYN,args->list[1]);
		Fix_Rm_Rp_info(0,0);
		DEBUG1("doaction: Printer '%s', RemotePrinter '%s', RemoteHost '%s'",
			Printer_DYN, RemotePrinter_DYN, RemoteHost_DYN );
		if( (s = safestrchr(args->list[1],'@')) ) *s = 0;
	} else if( Printer_DYN == 0 ){
		/* get the printer name */
		Get_printer();
		Fix_Rm_Rp_info(0,0);
	} else {
		Fix_Rm_Rp_info(0,0);
	}
	if( ISNULL(RemotePrinter_DYN) ){
		SNPRINTF( msg, sizeof(msg))
			_("Printer: %s - cannot get status from device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
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
		Set_DYN(&Auth_DYN, getenv("AUTH"));
	}
	if( Server ){
		DEBUG1("doaction: overriding Remotehost with '%s'", Server );
		Set_DYN(&RemoteHost_DYN, Server );
	}

	DEBUG1("lpc: RemotePrinter_DYN '%s', RemoteHost_DYN '%s'", RemotePrinter_DYN, RemoteHost_DYN );
	if( action == OP_DEFAULTS ){
		Dump_default_parms( 1, ".defaults", Pc_var_list );
	} else if( action == OP_LANG ){
		FPRINTF( STDOUT, _("Locale information directory '%s'\n"), LOCALEDIR );
		if( (s = getenv("LANG")) ){
			FPRINTF( STDOUT, _("LANG environment variable '%s'\n"), s );
			t = _("");
			if( t && *t ){
				FPRINTF( STDOUT, _("gettext translation information '%s'\n"), t );
			} else {
				Write_fd_str(1,_("No translation available\n"));
			}
			FPRINTF(STDERR, "Translation of '%s' is '%s'\n","TRANSLATION TEST", _("TRANSLATION TEST"));
		} else {
			FPRINTF( STDOUT, "LANG environment variable not set\n" );
		}
	} else if( action == OP_CLIENT || action == OP_SERVER ){
		if( action == OP_SERVER ){
			Is_server = 1;
			Setup_configuration();
			Get_printer();
		}
		Dump_default_parms( 1, ".defaults", Pc_var_list );
		Free_line_list(&l);
		Merge_line_list(&l,&Config_line_list, 0, 0, 0);
		Escape_colons( &l );
		s = Join_line_list_with_sep(&l,"\n :");
		Expand_percent( &s );
		if( s ){
			if( Write_fd_str( 1, ".config\n :" ) < 0 ) cleanup(0);
			if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
			Write_fd_str( 1, "\n" );
			free(s); s = 0;
		} else {
			if( Write_fd_str( 1, ".config\n" ) < 0 ) cleanup(0);
		}
		Free_line_list(&l);

		if( args->count > 1 ){
			for( argspos = 1; argspos < args->count; ++ argspos ){
				if(
					!safestrcasecmp(args->list[argspos], "all")
					|| !safestrcasecmp(args->list[argspos], _("all") )
					){
					Show_all_printcap_entries();
				} else {
					Set_DYN(&Printer_DYN,args->list[argspos]);
					if( Write_fd_str( 1,_("\n")) < 0 ) cleanup(0);
					if( Write_fd_str( 1,_("# Printcap Information\n")) < 0 ) cleanup(0);
					Show_formatted_info();
				}
			}
		} else if( !safestrcasecmp( Printer_DYN, "all" )
			|| !safestrcasecmp( Printer_DYN, _("all") ) ){
			Show_all_printcap_entries();
		} else {
			if( Write_fd_str( 1,_("\n")) < 0 ) cleanup(0);
			if( Write_fd_str( 1,_("# Printcap Information\n")) < 0 ) cleanup(0);
			Show_formatted_info();
		}
	} else if( action == OP_LPQ || action == OP_LPRM ){
		pid_t pid, result;
		plp_status_t status;
		if( args->count == 1 && Printer_DYN ){
			SNPRINTF(msg,sizeof(msg)) "-P%s", Printer_DYN );
			Add_line_list(args,msg,0,0,0);
			Check_max(args,1);
			args->list[args->count] = 0;
		} else if( args->count > 1 ){
			s = args->list[1];
			if( safestrcasecmp(s,"all")
			  || safestrcasecmp(s,_("all")) ){
				SNPRINTF(msg,sizeof(msg)) "-P%s", s );
			} else {
				strcpy(msg, "-a" );
			}
			if( s ) free(s);
			args->list[1] = safestrdup(msg,__FILE__,__LINE__);
		}
		if(DEBUGL1)Dump_line_list("ARGS",args);
		if( (pid = dofork(0)) == 0 ){
			/* we are going to close a security loophole */
			Full_user_perms();
			/* this would now be the same as executing LPQ as user */
			close_on_exec(3);
			execvp( args->list[0],args->list );
			DIEMSG( _("execvp failed - '%s'"), Errormsg(errno) );
			exit(0);
		} else if( pid < 0 ) {
			DIEMSG( _("fork failed - '%s'"), Errormsg(errno) );
		}
		while( (result = plp_waitpid(pid,&status,0)) != pid ){
			int err = errno;
			DEBUG1("lpc: waitpid(%d) returned %d, err '%s'",
				pid, result, Errormsg(err) );
			if( err == EINTR ) continue; 
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("doaction: waitpid(%d) failed"), pid);
		} 
		DEBUG1("lpc: system pid %d, exit status %s",
			result, Decode_status( &status ) );
	} else {
		Add_line_list(&l, Logname_DYN, Value_sep, 0, 0 );
		Add_line_list(&l, args->list[0], Value_sep, 0, 0);
		Remove_line_list(args, 0);
		if( args->count > 0 ) {
			Add_line_list(&l, RemotePrinter_DYN, Value_sep, 0, 0 );
			Remove_line_list(args, 0);
		}
		Merge_line_list(&l, args, 0, 0, 0 );
		Check_max(&l, 1 );
		l.list[l.count] = 0;
		fd = Send_request( 'C', REQ_CONTROL, l.list, Connect_timeout_DYN,
			Send_query_rw_timeout_DYN, 1 );
		if( fd > 0 ){
			shutdown( fd, 1 );
			while( (n = read(fd, msg, sizeof(msg))) > 0 ){
				if( (write(1,msg,n)) < 0 ) cleanup(0);
			}
		}
		close(fd);
	}
	Free_line_list(&l);
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/


 char LPC_optstr[] 	/* LPC options */
 = "AaD:P:S:VU:";

/* scan the input arguments, setting up values */

void Get_parms(int argc, char *argv[] )
{
	int option;

	while ((option = Getopt (argc, argv, LPC_optstr )) != EOF) {
		switch (option) {
		case 'A': Auth = 1; break;
		case 'a': Set_DYN(&Printer_DYN,"all"); break;
		case 'D': /* debug has already been done */
			Parse_debug( Optarg, 1 );
			break;
		case 'P': if( Optarg == 0 ) usage();
			Set_DYN(&Printer_DYN,Optarg); break;
		case 'V':
			++Verbose;
			break;
		case 'S':
			Server = Optarg;
			break;
		case 'U': Username_JOB = Optarg; break;
		default:
			usage();
		}
	}
	if( Verbose ) {
		FPRINTF( STDOUT, "%s\n", Version );
		if( Verbose > 1 ){
			char *s, *t;
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

 char *msg[] ={
 N_("usage: %s [-a][-Ddebuglevel][-Pprinter][-Shost][-Uusername][-V] [command]\n"),
 N_(" with no command, reads from STDIN\n"),
 N_("  -a           - alias for -Pall\n"),
 N_("  -Ddebuglevel - debug level\n"),
 N_("  -Pprinter    - printer\n"),
 N_("  -Pprinter@host - printer on lpd server on host\n"),
 N_("  -Shost       - connect to lpd server on host\n"),
 N_("  -Uuser       - identify command as coming from user\n"),
 N_("  -V           - increase information verbosity\n"),
 N_(" commands:\n"),
 N_(" active    (printer[@host])        - check for active server\n"),
 N_(" abort     (printer[@host] | all)  - stop server\n"),
 N_(" class     printer[@host] (class | off)      - show/set class printing\n"),
 N_(" disable   (printer[@host] | all)  - disable queueing\n"),
 N_(" debug     (printer[@host] | all) debugparms - set debug level for printer\n"),
 N_(" down      (printer[@host] | all)  - disable printing and queueing\n"),
 N_(" enable    (printer[@host] | all)  - enable queueing\n"),
 N_(" flush     (printer[@host] | all)  - flush cached status\n"),
 N_(" hold      (printer[@host] | all) (name[@host] | job | all)*   - hold job\n"),
 N_(" holdall   (printer[@host] | all)  - hold all jobs on\n"),
 N_(" kill      (printer[@host] | all)  - stop and restart server\n"),
 N_(" lpd       (printer[@host])        - get LPD PID \n"),
 N_(" lpq       (printer[@host] | all) (name[@host] | job | all)*   - invoke LPQ\n"),
 N_(" lprm      (printer[@host] | all) (name[@host]|host|job| all)* - invoke LPRM\n"),
 N_(" msg       printer message text  - set status message\n"),
 N_(" move      printer (user|jobid)* target - move jobs to new queue\n"),
 N_(" noholdall (printer[@host] | all)  - hold all jobs off\n"),
 N_(" printcap  (printer[@host] | all)  - report printcap values\n"),
 N_(" quit                              - exit LPC\n"),
 N_(" redirect  (printer[@host] | all) (printer@host | off )*       - redirect jobs\n"),
 N_(" redo      (printer[@host] | all) (name[@host] | job | all)*   - reprint jobs\n"),
 N_(" release   (printer[@host] | all) (name[@host] | job | all)*   - release jobs\n"),
 N_(" reread                            - LPD reread database information\n"),
 N_(" start     (printer[@host] | all)  - start printing\n"),
 N_(" status    (printer[@host] | all)  - status of printers\n"),
 N_(" stop      (printer[@host] | all)  - stop  printing\n"),
 N_(" topq      (printer[@host] | all) (name[@host] | job | all)*   - reorder jobs\n"),
 N_(" up        (printer[@host] | all) - enable printing and queueing\n"),
 N_("   diagnostic:\n"),
 N_("      defaultq               - show default queue for LPD server\n"),
 N_("      defaults               - show default configuration values\n"),
 N_("      lang                   - show current i18n (iNTERNATIONALIZATIONn) support\n"),
 N_("      client (printer | all) - client config and printcap information\n"),
 N_("      server (printer | all) - server config and printcap\n"),
	0} ;

void use_msg(void)
{
	int i;
	char *s;
	for( i = 0; (s = msg[i]); ++i ){
		if( i == 0 ){
			FPRINTF( STDERR, _(s), Name );
		} else {
			FPRINTF( STDERR, "%s", _(s) );
		}
	}
}
void usage(void)
{
	use_msg();
	Parse_debug("=",-1);
	FPRINTF( STDOUT, "%s\n", Version );
	exit(1);
}

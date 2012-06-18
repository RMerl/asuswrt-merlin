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
"$Id: lpr.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "gethostinfo.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "printjob.h"
#include "sendjob.h"
#include "lpd_jobs.h"

/**** ENDINCLUDE ****/

#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpr.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * main()
 * - top level of LPR Lite.  This is a cannonical method of handling
 *   input.  Note that we assume that the LPD daemon will handle all
 *   of the dirty work associated with formatting, printing, etc.
 * 
 * 1. get the debug level from command line arguments
 * 2. set signal handlers for cleanup
 * 3. get the Host computer Name and user Name
 * 4. scan command line arguments
 * 5. check command line arguments for consistency
 * 6. if we are spooling from STDIN, copy stdin to a file.
 * 7. if we have a list of files,  check each for access
 * 8. create a control file
 * 9. send control file to server
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	off_t job_size;
	char *s, *t, buffer[SMALLBUFFER], *send_to_pr = 0;
	struct job prjob;
	struct line_list opts, newargs;
	int attempt = 0;
	int n;

#ifndef NODEBUG
	Debug = 0;
#endif

	/* set signal handlers */
	Is_lpr = 1;
	Init_line_list( &newargs );
	Init_line_list( &opts );
	memset(&prjob, 0, sizeof(prjob) );
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) signal(SIGCHLD, SIG_DFL);
	(void) signal(SIGPIPE, SIG_IGN);

	/*
	 * set up the defaults
	 */
	Errorcode = 1;
	Initialize(argc, argv, envp, 'D' );
	Setup_configuration();
	Job_number = DbgTest;


	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */
	if( Auth_JOB && !getenv( "AUTH" ) ){
		FPRINTF(STDERR,
		_("authentication requested (-A option) and AUTH environment variable not set") );
		usage();
	}

	/* Note: we may need the open connection to the remote printer
		to get our IP address if it is not available */

    if(DEBUGL3){
		struct stat statb;
		int i;
        LOGDEBUG("lpr: after init open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }

 again:

	Free_job(&prjob);
	Get_printer();
	Fix_Rm_Rp_info(0,0);

	DEBUG1("lpr: Lpr_opts_DYN '%s', argc %d", Lpr_opts_DYN, argc );
	if( Lpr_opts_DYN ){
		int i, j;

		Split_cmd_line( &opts, Lpr_opts_DYN );
		Check_max( &newargs, argc+2+opts.count+2 );
		i = j = 0;
		
		newargs.list[newargs.count++] = argv[0];
		for( j = 0; j < opts.count; ++j ){
			newargs.list[newargs.count++] = opts.list[j];
		}
		for( j = 1; j < argc; ++j ){
			newargs.list[newargs.count++] = argv[j];
		}
		newargs.list[newargs.count] = 0;
		if(DEBUGL1)Dump_line_list("lpr - new options",&newargs );
		Optind = 0;
		Files.count = 0;
		Getopt(0,0,0);
		Get_parms(newargs.count, newargs.list);      /* scan input args */
		newargs.count = 0;
	}
	job_size = Make_job(&prjob);

    if(DEBUGL3){
		struct stat statb;
		int i;
        LOGDEBUG("lpr: after Make_job open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }

	/*
	 * Fix the rest of the control file
	 */
	if( job_size == 0 ){
		Free_job(&prjob);
		Errorcode = 1;
		FATAL(LOG_INFO)_("nothing to print"));
	}

	if( Check_for_rg_group( Logname_DYN ) ){
		Errorcode = 1;
		FATAL(LOG_INFO)_("cannot use printer - not in privileged group\n") );
	}

	if( Remote_support_DYN ) uppercase( Remote_support_DYN );
	if( safestrchr( Remote_support_DYN, 'R' ) == 0 ){
		Errorcode = 1;
		FATAL(LOG_INFO) _("no remote support for %s@%s"),
			RemotePrinter_DYN,RemoteHost_DYN );
	}

	/* we check to see if we need to do control file filtering */
	/* we do not do any translation of formats */
	s = 0;

	n = Find_flag_value( &prjob.info,DATAFILE_COUNT,Value_sep);
	if( Max_datafiles_DYN > 0 && n > Max_datafiles_DYN ){
		Errorcode = 1;
		FATAL(LOG_INFO) _("%d data files and maximum allowed %d"),
					n, Max_datafiles_DYN );
	}

	send_to_pr = 0;
	if( Direct_JOB ){
		/* check to see if we have a socket connection specified */
		send_to_pr = Printer_JOB;
	} else if( Direct_DYN ){
		send_to_pr = Lp_device_DYN;
	}
	Force_localhost_DYN = 0;
	if( send_to_pr ){
		Force_localhost_DYN = 0;
		Lpr_bounce_DYN = Lpr_bounce_JOB = 0;
		send_to_pr = safestrdup(send_to_pr,__FILE__,__LINE__);
		Expand_percent(&send_to_pr);
	}

	DEBUG1("lpr: send_to_pr '%s'", send_to_pr );

	if( Lpr_bounce_DYN || Lpr_bounce_JOB ){
		int tempfd;
		struct stat statb;
		char *tempfile, *old_lp_value;
		struct line_list *lp;

		if(DEBUGL2) Dump_job( "lpr - before filtering", &prjob );
		tempfd = Make_temp_fd(&tempfile);

		old_lp_value = safestrdup(Find_str_value( &PC_entry_line_list, "lp", Value_sep ),
			__FILE__,__LINE__);
		Set_str_value( &PC_entry_line_list, LP, tempfile );
		/* Print_job( output_device, status_device, job, timeout, poll_for_status ) */
		Print_job( tempfd, -1, &prjob, 0, 0, User_filter_JOB );
		Set_str_value( &PC_entry_line_list, LP, old_lp_value );
		if( old_lp_value ) free( old_lp_value ); old_lp_value = 0;

		close(tempfd);
		tempfd = Checkread( tempfile, &statb );
		if( tempfd < 0 ){
			Errorcode = JABORT;
			FATAL(LOG_INFO) _("Cannot open file '%s', %s"), tempfile, Errormsg( errno ) );
		}
		close(tempfd);
		DEBUG2("lpr: jobs size now %0.0f", (double)(statb.st_size));
		job_size = statb.st_size;
		Free_listof_line_list(&prjob.datafiles);
		lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
		memset(lp,0,sizeof(lp[0]));
		Check_max(&prjob.datafiles,1);
		prjob.datafiles.list[prjob.datafiles.count++] = (void *)lp;
		Set_str_value(lp,OPENNAME,tempfile);
		Set_str_value(lp,"N",_("(lpr_filter)"));
		Set_flag_value(lp,COPIES,1);
		Set_double_value(lp,SIZE,job_size);
		Fix_bq_format( 'f', lp );
		User_filter_JOB = 0;
	}

	if(DEBUGL1)Dump_job("lpr - before Fix_control",&prjob);
	Fix_control( &prjob, Control_filter_DYN, 0 );
	if(DEBUGL1)Dump_job("lpr - after Fix_control",&prjob);

	if( send_to_pr &&
		((!strchr(send_to_pr,'@') && strchr(send_to_pr,'%'))
			|| (send_to_pr[0] == '/') || strchr(send_to_pr,'|')) ){
		int fd, pid, status_fd, poll_for_status;
		char *id;

		SETSTATUS(&prjob)"destination '%s'", send_to_pr );
		Errorcode = 0;
		fd = pid = status_fd = poll_for_status = 0;
		fd = Printer_open(send_to_pr, &status_fd, &prjob,
			Lpr_send_try_DYN, Connect_interval_DYN, Max_connect_interval_DYN,
			Connect_grace_DYN, Connect_timeout_DYN, &pid, &poll_for_status );

		/* note: we NEVER return fd == 0 or horrible things have happened */
		DEBUG1("lpr: fd %d", fd );
		if( fd <= 0 ){
			Errorcode = JFAIL;
			goto exit;
		}
		id = Find_str_value(&prjob.info,IDENTIFIER,Value_sep);
		SETSTATUS(&prjob)"transferring job '%s'", id );
		/* Print_job( output_device, status_device, job, timeout, poll_for_status, filter ) */
		Set_str_value( &PC_entry_line_list, LP, s );
		Errorcode = Print_job( fd, status_fd, &prjob, Send_job_rw_timeout_DYN, poll_for_status, User_filter_JOB );
		/* we close close device */
		DEBUG1("lpr: shutting down fd %d", fd );

		fd = Shutdown_or_close( fd );
		DEBUG1("lpr: after shutdown fd %d, status_fd %d", fd, status_fd );
		if( status_fd > 0 ){
			/* we shut down this connection as well */
			shutdown(status_fd,1);
			/* we wait for eof on status_fd */
			buffer[0] = 0;
			Get_status_from_OF(&prjob,"LP",pid,
				status_fd, buffer, sizeof(buffer)-1, Send_job_rw_timeout_DYN, 0, 0, 0 );
		}
		if( fd > 0 ) close( fd ); fd = -1;
		if( status_fd > 0 ) close( status_fd ); status_fd = -1;
		if( pid > 0 ){
			SETSTATUS(&prjob)"waiting for printer filter to exit");
			Errorcode = Wait_for_pid( pid, "LP", 0, Send_job_rw_timeout_DYN );
		}
		DEBUG1("lpr: status %s", Server_status(Errorcode) );
	} else {
		Errorcode = 0;
		attempt = 1;
		do {
			if( Errorcode ){
				if(DEBUGL1)Dump_job("lpr - after error",&prjob);
				buffer[0] = 0;
				SNPRINTF(buffer,sizeof(buffer))
					_("Status Information, attempt %d:\n"), attempt);
				if( Lpr_send_try_DYN ){
					n = strlen(buffer)-2;
					SNPRINTF(buffer+n,sizeof(buffer)-n)
					_(" of %d:\n"), Lpr_send_try_DYN);
				}
				Write_fd_str(2,buffer);
				s = Join_line_list(&Status_lines,"\n ");
				if( (t = safestrrchr(s,' ')) ) *t = 0;
				Write_fd_str(2,s);
				if(s) free(s); s = 0;
				Init_line_list( &Status_lines );
				++attempt;
				n = Connect_interval_DYN + Connect_grace_DYN;
				if( n > 0 ){
					buffer[0] = 0;
					SNPRINTF(buffer,sizeof(buffer))
						_("Waiting %d seconds before retry\n"), n);
					Write_fd_str(2,buffer);
					plp_sleep( n );
				}
				Errorcode = 0;
			}
			Errorcode = Send_job( &prjob, &prjob, Connect_timeout_DYN,
				Connect_interval_DYN,
				Max_connect_interval_DYN,
				Send_job_rw_timeout_DYN, User_filter_JOB );
		} while( Errorcode && (Lpr_send_try_DYN == 0 || attempt < Lpr_send_try_DYN) );
	}

  exit:
	if( send_to_pr ) free(send_to_pr); send_to_pr = 0;
	if( Errorcode ){
		Errorcode = 1;
		if(DEBUGL1)Dump_job("lpr - after error",&prjob);
		buffer[0] = 0;
		SNPRINTF(buffer,sizeof(buffer))
			_("Status Information, attempt %d:\n"), attempt);
		if( Lpr_send_try_DYN ){
			n = strlen(buffer)-2;
			SNPRINTF(buffer+n,sizeof(buffer)-n)
			_(" of %d:\n"), Lpr_send_try_DYN);
		}
		s = Join_line_list(&Status_lines,"\n ");
		if( (t = safestrrchr(s,' ')) ) *t = 0;
		Write_fd_str(2,s);
		if(s) free(s); s = 0;
		cleanup(0);
	}

	if( LP_mode_JOB && !Silent_JOB ){
		char *id;
		int n;
		char msg[SMALLBUFFER];
		id = Find_str_value(&prjob.info,IDENTIFIER,Value_sep);
		if( id ){
			SNPRINTF(msg,sizeof(msg)-1)_("request id is %s\n"), id );
		} else {
			n = Find_decimal_value(&prjob.info,NUMBER,Value_sep);
			SNPRINTF(msg,sizeof(msg)-1)_("request id is %d\n"), n );
		}
		Write_fd_str(1, msg );
	}

	/* the dreaded -r (remove files) option */
	if( Removefiles_JOB && !Errorcode ){
		int i;
		/* eliminate any possible game playing */
		To_user();
		for( i = 0; i < Files.count; ++i ){
			if( unlink( Files.list[i] ) == -1 ){
				WARNMSG(_("Error unlinking '%s' - %s"),
					Files.list[i], Errormsg( errno ) );

			}
		}
	}

	if( Job_number ){
		SNPRINTF(buffer,sizeof(buffer))_("Done %d\n"), Job_number);
		Write_fd_str(1,buffer);
		++Job_number;
		goto again;
	}
	Free_line_list( &newargs );
	Free_line_list( &opts );
	Free_job(&prjob);
	Free_line_list(&Files);
	cleanup(0);
	return(0);
}


/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 void usage(void);


 char LPR_optstr[]    /* LPR options */
 = "1:2:3:4:#:ABC:D:F:GJ:K:NP:QR:T:U:VX:YZ:bcdfghi:klm:nprstvw:" ;
 char LPR_bsd_optstr[]    /* LPR options */
 = "1:2:3:4:#:ABC:D:F:GJ:K:NP:QR:T:U:VX:YZ:bcdfghi:klmnprstvw:" ;
 char LP_optstr[]    /* LP options */
 = 	"ckmprswd:BD:f:GH:n:o:P:q:S:t:T:X:Yy:";

void Get_parms(int argc, char *argv[] )
{
	int option, i;
	char *name, *s;

	Verbose = 0;
	if( argv[0] && (name = safestrrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && safestrcmp( name, "lp" ) == 0 ){
		LP_mode_JOB = 1;
	}
	DEBUG1("Get_parms: LP_mode %d", LP_mode_JOB );
	if( LP_mode_JOB ){
		while( (option = Getopt( argc, argv, LP_optstr)) != EOF ){
		DEBUG1("Get_parms: option %c", option );
		switch( option ){
		case 'A':   Auth_JOB = 1; break;
		case 'B':   Lpr_bounce_JOB = 1; break;
		case 'c':	break;	/* use symbolic link */
		case 'k':	Lpr_zero_file_JOB = 1; break;	/* send input with 0 length */
		case 'm':	/* send mail */
					Mailname_JOB = getenv( "USER" );
					if( Mailname_JOB == 0 ){
						DIEMSG( _("USER environment variable undefined") );
					}
					break;
		case 'p':	break;	/* ignore notification */
		case 'r':	break;	/* ignore this option */
		case 's':	Verbose = 0; Silent_JOB = 1; break;	/* suppress messages flag */
		case 'w':	break;	/* no writing of message */
		case 'd':	Set_DYN(&Printer_DYN, Optarg); /* destination */
					Printer_JOB = Optarg;
					break;
		case 'D': 	Parse_debug(Optarg,1);
					break;
		case 'f':	Classname_JOB = Optarg;
					break;
		case 'H':	/* special handling - ignore */
					break;
		case 'n':	Copies_JOB = atoi( Optarg );	/* copies */
					if( Copies_JOB <= 0 ){
						DIEMSG( _("-ncopies -number of copies must be greater than 0\n"));
					}
					break;
		case 'o':	if( safestrcasecmp( Optarg, "nobanner" ) == 0
						|| safestrcasecmp( Optarg,_("nobanner") ) == 0 ){
						No_header_JOB = 1;
					} else if( safestrncasecmp( Optarg, "width", 5 ) == 0
						|| safestrncasecmp( Optarg,_("width"), 5 ) == 0 ){
						s = safestrchr( Optarg, '=' );
						if( s ){
							Pwidth_JOB = atoi( s+1 );
						}
					} else {
						/* pass as Zopts */
						if( Zopts_JOB ){
							s = Zopts_JOB;
							Zopts_JOB = safestrdup3(s,",",Optarg,
								__FILE__,__LINE__);
							free(s);
						} else {
							Zopts_JOB = safestrdup(Optarg,
								__FILE__,__LINE__);
						}
					}
					break;
		case 'P':	break;	/* ignore page lis */
		case 'q':	Priority_JOB = 'Z' - atoi(Optarg);	/* get priority */
					if(Priority_JOB < 'A' ) Priority_JOB = 'A';
					if(Priority_JOB > 'Z' ) Priority_JOB = 'Z';
					break;
		/* pass these as Zopts */
		case 'S':
		case 'T':
		case 'y':
					/* pass as Zopts */
					if( Zopts_JOB ){
						s = Zopts_JOB;
						Zopts_JOB = safestrdup3(s,",",Optarg,
							__FILE__,__LINE__);
						free(s);
					} else {
						Zopts_JOB = safestrdup(Optarg,
							__FILE__,__LINE__);
					}
					break;
		case 't':
				Check_str_dup( option, &Jobname_JOB, Optarg, M_JOBNAME);
				break;
		case 'X':
				Check_str_dup( option, &User_filter_JOB, Optarg, M_JOBNAME);
				break;
		case 'Y': Direct_JOB = 1; break;
		default:
			usage();
		    break;
		}
		}
	} else {
		while( (option = Getopt (argc, argv, LPR_bsd_DYN?LPR_bsd_optstr:LPR_optstr )) != EOF ) {
		DEBUG1("Get_parms: option %c", option );
		switch( option ){
		case 'A':   Auth_JOB = 1; break;
		case 'B':   Lpr_bounce_JOB = 1; break;
		case '1':
		    Check_str_dup( option, &Font1_JOB, Optarg, M_FONT);
			break;
		case '2':
		    Check_str_dup( option, &Font2_JOB, Optarg, M_FONT);
			break;
		case '3':
		    Check_str_dup( option, &Font3_JOB, Optarg, M_FONT);
			break;
		case '4':
		    Check_str_dup( option, &Font4_JOB, Optarg, M_FONT);
			break;
		case 'C':
		    Check_str_dup( option, &Classname_JOB, Optarg,
			   M_CLASSNAME);
		    break;
		case 'D': 	Parse_debug(Optarg,1);
			break;
		case 'F':
		    if( safestrlen (Optarg) != 1 ){
		        DIEMSG( _("bad -F format string '%s'\n"), Optarg);
		    }
		    if( Format_JOB ){
		        DIEMSG( _("duplicate format specification -F%s\n"), Optarg);
		    } else {
		        Format_JOB = *Optarg;
		    }
		    break;
		case 'J':
		    Check_str_dup( option, &Jobname_JOB, Optarg, M_JOBNAME);
		    break;
		case 'K':
		case '#':
		    Check_int_dup( option, &Copies_JOB, Optarg, 0);
			if( Copies_JOB <= 0 ){
		        DIEMSG( _("-Kcopies -number of copies must be greater than 0\n"));
			}
		    break;
		case 'N':
			Check_for_nonprintable_DYN = 0;
			break;
		case 'P':
		    Printer_JOB = Optarg;
		    Set_DYN(&Printer_DYN,Optarg);
		    break;
		case 'Q':
			Use_queuename_flag_DYN = 1;
			break;
		case 'R':
		    Check_str_dup( option, &Accntname_JOB, Optarg, M_ACCNTNAME );
		    break;
		case 'T':
		    Check_str_dup( option, &Prtitle_JOB, Optarg, M_PRTITLE);
		    break;
		case 'U': Check_str_dup( option, &Username_JOB, Optarg, M_BNRNAME );
		    break;
		case 'V':
			++Verbose;
		    break;
		case 'X':
				Check_str_dup( option, &User_filter_JOB, Optarg, M_JOBNAME);
				break;
		case 'Y': Direct_JOB = 1; break;
		case 'o': /* same as Z */
		case 'Z':
			if( Zopts_JOB ){
				s = Zopts_JOB;
				Zopts_JOB = safestrdup3(s,",",Optarg,
					__FILE__,__LINE__);
				free(s);
			} else {
				Zopts_JOB = safestrdup(Optarg,
					__FILE__,__LINE__);
			}
		    break;
		case 'k':	Lpr_zero_file_JOB = 1; break;	/* send input with 0 length */
		case 'l':
		case 'b':
		    Binary_JOB = 1;
		    break;
		case 'h':
		    Check_dup( option, &No_header_JOB);
		    break;
		case 'i':
		    Check_int_dup( option, &Indent_JOB, Optarg, 0);
		    break;
		case 'm':
		    /*
		     * -m Mailname
		     */
			if( LPR_bsd_DYN ){
					Mailname_JOB = getenv( "USER" );
					if( Mailname_JOB == 0 ){
						DIEMSG( _("USER environment variable undefined") );
					}
					break;
			}
			if( Optarg[0] == '-' ){
				DIEMSG( _("Missing mail name") );
			} else {
				Mailname_JOB = Optarg;
			}
		    break;
		case 'c':
		case 'd':
		case 'f':
		case 'g':
		case 'n':
		case 'p':
		case 't':
		case 'v':
		    if( Format_JOB ){
		        DIEMSG( _("duplicate format specification -%c\n"), option);
		    } else {
		        Format_JOB = option;
		    }
		    break;
		case 'w':
		    Check_int_dup( option, &Pwidth_JOB, Optarg, 0);
		    break;

		/* Throw a sop to the whiners - let them wipe themselves out... */
		/* remove files */
		case 'r':
			Removefiles_JOB = 1;
			break;
		case 's':
			/* symbolic link - quietly ignored */
			break;
		default:
			usage();
		    break;
		}
		}
	}

	/*
	 * set up the Parms[] array
	 */
	for( i = Optind; i < argc; ++i ){
		Add_line_list(&Files,argv[i],0,0,0);
	}
	if( Verbose ){
		if( Verbose > 1 ){
			 Printlist( Copyright, 1 );
		} else {
			Write_fd_str( 2, Version );
			Write_fd_str( 2, "\n" );
		}
	}
}


 char *LPR_msg [] =
{
 N_("Usage: %s [-Pprinter[@host]] [-A] [-B] [-Cclass] [-Fformat] [-G] [-Jinfo]\n"),
 N_("   [-(K|#)copies] [-Q] [-Raccountname]  [-Ttitle]  [-Uuser[@host]] [-V]\n"),
 N_("   [-Zoptions] [-b] [-m mailaddr] [-h] [-i indent] [-l] [-w width ] [-r]\n"),
 N_("   [-Ddebugopt ] [--] [ filenames ...  ]\n"),
 N_(" -A          - use authentication specified by AUTH environment variable\n"),
 N_(" -B          - filter files and reduce job to single file before sending\n"),
 N_(" -C class    - job class\n"),
 N_(" -D debugopt - debugging flags\n"),
 N_(" -F format   - job format\n"),
 N_("   -b,-l        - binary or literal format\n"),
 N_("    c,d,f,g,l,m,p,t,v are also format options\n"),
 N_(" -G          - filter individual job files before sending\n"),
 N_(" -J info     - banner and job information\n"),
 N_(" -K copies, -# copies   - number of copies\n"),
 N_(" -P printer[@host] - printer on host\n"),
 N_(" -Q          - put 'queuename' in control file\n"),
 N_(" -Raccntname - accounting information\n"),
 N_(" -T title    - title for 'pr' (-p) formatting\n"),
 N_(" -U username - override user name (restricted)\n"),
 N_(" -V          - Verbose information during spooling\n"),
 N_(" -X path     - user specified filter for job files\n"),
 N_(" -Y          - connect and send to TCP/IP port (direct mode)\n"),
 N_(" -Z options  - options to pass to filter\n"),
 N_(" -h          - no header or banner page\n"),
 N_(" -i indent   - indentation\n"),
 N_(" -k          - do not use tempfile when sending to server\n"),
 N_(" -m mailaddr - mail final status to mailaddr\n"),
 N_(" -r          - remove files after spooling\n"),
 N_(" -w width    - width to use\n"),
 N_(" --          - end of options, files follow\n"),
 N_(" filename '-'  reads from STDIN\n"),
 N_(" PRINTER, LPDEST, NPRINTER, NGPRINTER environment variables set default printer.\n"),
 0 };

 char *LP_msg [] = {
 N_("Usage: %s [-A] [-B] [-c] [-G] [-m] [-p] [-s] [-w] [-d printer@[host]]\n"),
 N_("  [-f form-name] [-H special-handling]\n"),
 N_("  [-n number] [-o options] [-P page-list]\n"),
 N_("  [-q priority-level] [-S character-set]\n"),
 N_("  [-S print-wheel] [-t title]\n"),
 N_("  [-T content-type [-r]] [-y mode-list]\n"),
 N_("  [-Ddebugopt ] [ filenames ...  ]\n"),
 N_(" lp simulator using LPRng,  functionality may differ slightly\n"),
 N_(" -A          - use authentication specified by AUTH environment variable\n"),
 N_(" -B          - filter files and reduce job to single file before sending\n"),
 N_(" -c          - (make copy before printing - ignored)\n"),
 N_(" -d printer[@host]  - printer on host\n"),
 N_(" -D debugflags  - debugging flags\n"),
 N_(" -f formname - first letter used as job format\n"),
 N_(" -G          - filter individual job files before sending\n"),
 N_(" -H handling - (passed as -Z handling)\n"),
 N_(" -m          - mail sent to $USER on completion\n"),
 N_(" -n copies   - number of copies\n"),
 N_(" -o option     nobanner, width recognized\n"),
 N_("               (others passed as -Z option)\n"),
 N_(" -P pagelist - (print page list - ignored)\n"),
 N_(" -p          - (notification on completion - ignored)\n"),
 N_(" -q          - priority - 0 -> Z (highest), 25 -> A (lowest)\n"),
 N_(" -s          - (suppress messages - ignored)\n"),
 N_(" -S charset  - (passed as -Z charset)\n"),
 N_(" -t title    - job title\n"),
 N_(" -T content  - (passed as -Z content)\n"),
 N_(" -w          - (write message on completion - ignored)\n"),
 N_(" -X path     - user specified filter for job files\n"),
 N_(" -Y          - connect and send to TCP/IP port (direct mode)\n"),
 N_(" -y mode     - (passed as -Z mode)\n"),
 N_(" --          - end of options, files follow\n"),
 N_(" filename '-'  reads from STDIN\n"),
 N_(" PRINTER, LPDEST, NGPRINTER, NPRINTER environment variables set default printer.\n"),
	0 };

void prmsg( char **msg )
{
	int i;
	char *s;
	for( i = 0; (s = msg[i]); ++i ){
		if(i == 0 ){
			FPRINTF( STDERR,_(s), Name );
		} else {
			FPRINTF( STDERR, "%s",_(s) );
		}
	}
}

void usage(void)
{
	if(LP_mode_JOB ){
		prmsg( LP_msg );
	} else {
		prmsg( LPR_msg );
	}
	Parse_debug("=",-1);
	FPRINTF( STDERR, "%s\n", Version );
	exit(1);
}


/***************************************************************************
 * Make_job Parms()
 * 1. we determine the name of the printer - Printer_DYN variable
 * 2. we determine the host name to be used - RemoteHost_DYN variable
 * 3. check the user name for consistency:
 * 	We have the user name from the environment
 * 	We have the user name from the -U option
 *     Allow override if we are root or some silly system (like DOS)
 * 		that does not support multiple users
 ***************************************************************************/


 void get_job_number( struct job *job );
 double Copy_STDIN( struct job *job );
 double Check_files( struct job *job );

/***************************************************************************
 * Commentary:
 * The struct control_file{}  data structure contains fields that point to
 * complete lines in the control file, i.e.- 'Jjobname', 'Hhostname'
 * We set up this information in a data structure.
 * Note that this is specific to the LPR program
 *
 * Make_job()
 * 1. Get the control file number and name information
 * 2. scan the set of variables,  and determine how much space is needed.
 * 3. scan the data files,  and determine how much space is needed
 * 4. allocate the space.
 * 5. Copy variables to the allocated space,  setting up pointers in the
 *    control_file data structure.
 **************************************************************************/

int Make_job( struct job *job )
{
	char nstr[SMALLBUFFER];	/* information */
	struct jobwords *keys;	/* keyword entry in the parameter list */
	char *s, *name;		/* buffer where we allocate stuff */
	void *p;
	char *originate_hostname = 0;
	int i, n;
	double job_size = 0;

	if( Auth_JOB ){
		Set_DYN(&Auth_DYN, getenv("AUTH") );
	}

	if(DEBUGL4)Dump_line_list("Make_job - PC_entry",&PC_entry_line_list );
	if(DEBUGL4)Dump_parms("Make_job",Pc_var_list);
	if(DEBUGL4)Dump_line_list("Make_job - job at start",&job->info );

	/* check for priority in range */
	if( Priority_JOB == 0 && Classname_JOB
		&& !Break_classname_priority_link_DYN ) Priority_JOB = cval(Classname_JOB);
	if( Priority_JOB == 0 && Default_priority_DYN ) Priority_JOB = cval(Default_priority_DYN);
	if( Priority_JOB == 0 ) Priority_JOB = 'A';
	if( islower(Priority_JOB) ) Priority_JOB = toupper( Priority_JOB );
	if( !isupper( Priority_JOB ) ){
		DIEMSG(
		_("Priority (first letter of Class) not 'A' (lowest) to 'Z' (highest)") );
	}

	SNPRINTF(nstr,sizeof(nstr))"%c",Priority_JOB);
	Set_str_value(&job->info,PRIORITY,nstr);

	/* fix up the Classname_JOB 'C' option */

	if( Classname_JOB == 0 ){
		if( Backwards_compatible_DYN ){
			Classname_JOB = ShortHost_FQDN;
		} else {
			SNPRINTF(nstr,sizeof(nstr))"%c",Priority_JOB);
			Classname_JOB = nstr;
		}
	}
	Set_str_value(&job->info,CLASS,Classname_JOB);

	if( Files.count == 1 && !safestrcmp("-", Files.list[0]) ){
		Files.count = 0;
	}
	/* fix up the jobname */
	if( Jobname_JOB == 0 ){
		if( Files.count == 0 ){
			Set_str_value(&job->info,JOBNAME,_("(STDIN)") );
		} else {
			name = 0;
			for( i = 0; i < Files.count; ++i ){
				s = Files.list[i];
				if( safestrcmp(s, "-" ) == 0 ) s = _("(STDIN)");
				name = safeextend3(name,name?",":"",s,__FILE__,__LINE__);
			}
			Set_str_value(&job->info,JOBNAME, name );
			if( name ) free(name); name = 0;
		}
	} else {
		Set_str_value(&job->info,JOBNAME,Jobname_JOB );
	}
	if(DEBUGL4)Dump_line_list("Make_job - after jobname",&job->info);

	/* fix up the banner name.
	 * if you used the -U option,
     *   check to see if you have root permissions
	 *   set to -U value
	 * else set to log name of user
     * if No_header suppress banner
	 */
	if( Username_JOB ){
		/* check to see if you were root */
		if( 0 != OriginalRUID ){
			struct line_list user_list;
			char *str, *t;
			struct passwd *pw;
			int found;
			uid_t uid;

			DEBUG2("Make_job: checking '%s' for -U perms",
				Allow_user_setting_DYN );
			Init_line_list(&user_list);
			Split( &user_list, Allow_user_setting_DYN,File_sep,0,0,0,0,0,0);
			
			found = 0;
			for( i = 0; !found && i < user_list.count; ++i ){
				str = user_list.list[i];
				DEBUG2("Make_job: checking '%s'", str );
				uid = strtol( str, &t, 10 );
				if( str == t || *t ){
					/* try getpasswd */
					pw = getpwnam( str );
					if( pw ){
						uid = pw->pw_uid;
					}
				}
				DEBUG2( "Make_job: uid '%d'", uid );
				found = ( uid == OriginalRUID );
				DEBUG2( "Make_job: found '%d'", found );
			}
			if( !found ){
				DEBUG1( _("-U (username) can only be used by ROOT") );
				Username_JOB = 0;
			}
		}
	}
	if( Username_JOB ){
		Clean_meta(Username_JOB);
		if( (originate_hostname = strchr(Username_JOB,'@')) ){
			*originate_hostname++ = 0;
			for( s = originate_hostname; cval(s); ++s ){
				if( isspace(cval(s)) ){
					*s = '_';
				}
			}
			if( (s = Find_fqdn( &LookupHost_IP, originate_hostname )) == 0 ){
				Errorcode = JABORT;
				FATAL(LOG_ERR) _("Get_local_host: '%s' FQDN name not found!"), originate_hostname );
			} else {
				originate_hostname = s;
			}
		}
		Set_DYN(&Logname_DYN, Username_JOB );
	}
	if( !originate_hostname ) originate_hostname = FQDNHost_FQDN;
	Bnrname_JOB = Logname_DYN;
	if( No_header_JOB || Suppress_header_DYN ){
		Bnrname_JOB = 0;
	}
	Set_str_value(&job->info,BNRNAME, Bnrname_JOB );

	/* check the format */

	DEBUG1("Make_job: before checking format '%c'", Format_JOB );
	if( Binary_JOB ){
		Format_JOB = 'l';
	}
	if( Format_JOB == 0 && Default_format_DYN ) Format_JOB = *Default_format_DYN;
	if( Format_JOB == 0 ) Format_JOB = 'f';
	if( isupper(Format_JOB) ) Format_JOB = tolower(Format_JOB);

	DEBUG1("Make_job: after checking format '%c'", Format_JOB );
	if( safestrchr( "aios", Format_JOB )
		|| (Formats_allowed_DYN && !safestrchr( Formats_allowed_DYN, Format_JOB ) )){
		DIEMSG( _("Bad format specification '%c'"), Format_JOB );
	}

	SNPRINTF(nstr,sizeof(nstr))"%c",Format_JOB);
	Set_str_value(&job->info,FORMAT,nstr);
	/* check to see how many files you want to print- limit of 52 */
	if( Max_datafiles_DYN > 0 && Files.count > Max_datafiles_DYN ){
		DIEMSG( _("Sorry, can only print %d files at a time, split job up"), Max_datafiles_DYN);
	}
	if( Copies_JOB == 0 ){
		Copies_JOB = 1;
	}
	if( Max_copies_DYN && Copies_JOB > Max_copies_DYN ){
		DIEMSG( _("Maximum of %d copies allowed"), Max_copies_DYN );
	}
	Set_flag_value(&job->info,COPIES,Copies_JOB);

	/* check the for the -Q flag */
	DEBUG1("Make_job: 'qq' flag %d, queue '%s', force_queuename '%s'",
		Use_queuename_flag_DYN, Queue_name_DYN, Force_queuename_DYN );
	if( Use_queuename_flag_DYN ){
		Set_str_value(&job->info,QUEUENAME,Queue_name_DYN);
	}
	if( Force_queuename_DYN ){
		Set_str_value(&job->info,QUEUENAME,Force_queuename_DYN);
	}

	get_job_number(job);

	Set_str_value(&job->info,FROMHOST,originate_hostname);
	if( isdigit(cval(originate_hostname)) ){
		s = safestrdup2("ADDR",originate_hostname,__FILE__,__LINE__);
		Set_str_value(&job->info,FILE_HOSTNAME,s);
		if( s ) free(s); s = 0;
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,originate_hostname);
	}

	/* we put the option strings in the buffer */
	for( keys = Lpr_parms; keys->key; ++keys ){
		DEBUG2("Make_job: key '%s', maxlen %d, use '%s'",
			keys->keyword?*keys->keyword:0,keys->maxlen,keys->key);
		s = 0;
		/* see if we already have a value for this parameter.  If not,
			then we set it
		*/
		if( keys->keyword ){
			s = Find_str_value(&job->info,*keys->keyword,Value_sep);
		}
		p = keys->variable;
		nstr[0] = 0;
		n = 0;
		switch( keys->type ){
		case INTEGER_K:
			if( s ){
				n = strtol(s,0,0);
			} else if( p ){
				n = *(int *)p;
			}
			if( n ) Set_decimal_value(&job->info,keys->key,n);
			break;
		case STRING_K:
			if( s == 0 && p ) s = *(char **)p;
			if( s ) Set_str_value(&job->info,keys->key,s);
			break;
		default: break;
		}
	}
	if(DEBUGL2)Dump_job("Make_job - job after", job );

	/*
	 * copy from standard in?
	 */

	/* now we check to see if we have zero length flag */
	if( Lpr_zero_file_JOB ){
		if( Auth_JOB || Auth_DYN ){
			DIEMSG( _("authentication conficts with -k option"));
		}
		if( Send_block_format_DYN ){
			DIEMSG( _("send_block_format configuration option conficts with -k option"));
		}
		if( Send_data_first_DYN ){
			DIEMSG( _("send_data_first configuration option conficts with -k option"));
		}
		if(Copies_JOB > 1 ){
			DIEMSG( _("multiple copies conficts with -k option"));
		}
		if(Files.count ){
			DIEMSG( _("files on command line conflicts with -k option"));
		}
	}
	if( Files.count == 0 ){
		if( Lpr_zero_file_JOB || Direct_JOB ){
			struct line_list *lp;
			lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
			memset(lp,0,sizeof(lp[0]));
			Check_max(&job->datafiles,1);
			job->datafiles.list[job->datafiles.count++] = (void *) lp;
			Set_str_value(lp,"N","(STDIN)");
			Set_flag_value(lp,COPIES,1);
			SNPRINTF(nstr,sizeof(nstr))"%c",Format_JOB);
			Set_str_value(lp,FORMAT,nstr);
			Set_double_value(lp,SIZE,0 );
			job_size = 1;	/* make checker happy */
		} else {
			job_size = Copy_STDIN( job );
			job_size *= Copies_JOB;
		}
	} else {
		/*
		 * check to see that the input files are printable
		 */
		job_size = Check_files( job );
		job_size *= Copies_JOB;
	}

	if(DEBUGL2) Dump_job( "Make_job - final value", job );
	return( job_size );
}

/**************************************************************************
 * int get_job_number();
 * - get an integer value for the job number
 **************************************************************************/

void get_job_number( struct job *job )
{
	int number = Job_number;
	if( number == 0 ) number = getpid();
	Fix_job_number( job, number );
}

 struct jobwords Lpr_parms[]
 = {
{ 0,  STRING_K , &Accntname_JOB, M_ACCNTNAME, "R" },
{ &BNRNAME,  STRING_K , &Bnrname_JOB, M_BNRNAME, "L" },
{ &CLASS,  STRING_K , &Classname_JOB, M_CLASSNAME, "C" },
{ 0,  STRING_K , &Font1_JOB, M_FONT, "1" },
{ 0,  STRING_K , &Font2_JOB, M_FONT, "2" },
{ 0,  STRING_K , &Font3_JOB, M_FONT, "3" },
{ 0,  STRING_K , &Font4_JOB, M_FONT, "4" },
{ &FROMHOST,  STRING_K , &FQDNHost_FQDN, M_FROMHOST, "H" },
{ 0,  INTEGER_K , &Indent_JOB, M_INDENT, "I" },
{ &JOBNAME,  STRING_K , &Jobname_JOB, M_JOBNAME, "J" },
{ &LOGNAME,  STRING_K , &Logname_DYN, M_BNRNAME, "P" },
{ 0,  STRING_K , &Mailname_JOB, M_MAILNAME, "M" },
{ 0,  STRING_K , &Prtitle_JOB, M_PRTITLE, "T" },
{ 0,  INTEGER_K , &Pwidth_JOB, M_PWIDTH, "W" },
{ 0,  STRING_K , &Zopts_JOB, M_ZOPTS, "Z" },
{ 0,0,0,0,0 }
} ;


/***************************************************************************
 * off_t Copy_STDIN()
 * 1. we get the name of a temporary file
 * 2. we open the temporary file and read from STDIN until we get
 *    no more.
 * 3. stat the  temporary file to prevent games
 ***************************************************************************/

double Copy_STDIN( struct job *job )
{
	int fd, count, printable = 1;
	double size = 0;
	char *tempfile;
	struct line_list *lp;
	struct stat statb;
	char buffer[LARGEBUFFER];

	/* get a tempfile */
	if( Copies_JOB == 0 ) Copies_JOB = 1;
	fd = Make_temp_fd( &tempfile );

	if( fd < 0 ){
		LOGERR_DIE(LOG_INFO) _("Make_temp_fd failed") );
	} else if( fd == 0 ){
		DIEMSG( _("You have closed STDIN! cannot pipe from a closed connection"));
	}
	DEBUG1("Temporary file '%s', fd %d", tempfile, fd );
	size = 0;
	while( (count = read( 0, buffer, sizeof(buffer))) > 0 ){
		if( write( fd, buffer, count ) < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) _("Copy_STDIN: write to temp file failed"));
		}
	}
	if( fstat( fd, &statb ) != 0 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) _("Copy_STDIN: stat of temp fd '%d' failed"), fd);
	}
	printable = Check_lpr_printable( tempfile, fd, &statb, Format_JOB );
	if( printable ){
		size = statb.st_size;
		lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
		memset(lp,0,sizeof(lp[0]));
		Check_max(&job->datafiles,1);
		job->datafiles.list[job->datafiles.count++] = (void *) lp;
		Set_str_value(lp,"N","(STDIN)");
		Set_str_value(lp,OPENNAME,tempfile);
		Set_str_value(lp,TRANSFERNAME,tempfile);
		Set_flag_value(lp,COPIES,1);
		SNPRINTF(buffer,sizeof(buffer))"%c",Format_JOB);
		Set_str_value(lp,FORMAT,buffer);
		Set_double_value(lp,SIZE,size);
	} else {
		size = 0;
	}
	close(fd);
	return( size );
}

/***************************************************************************
 * off_t Check_files( char **files, int filecount )
 * 2. check each of the input files for access
 * 3. stat the files and get the size
 * 4. Check for printability
 * 5. Put information in the data_file{} entry
 ***************************************************************************/

double Check_files( struct job *job )
{
	double size = 0;
	int i, fd, printable = 1;
	struct stat statb;
	char *s, *tempfile;
	char buffer[SMALLBUFFER];
	struct line_list *lp;

	for( i = 0; i < Files.count; ++i){
		tempfile = s = Files.list[i];
		DEBUG2( "Check_files: doing '%s'", s );
		if( safestrcmp( s, "-" ) == 0 ){
			size += Copy_STDIN( job );
			continue;
		}
		fd = Checkread( s, &statb );
		if( fd < 0 ){
			WARNMSG( _("Cannot open file '%s', %s"), s, Errormsg( errno ) );
			continue;
		}
		printable = 1;
		if( User_filter_JOB == 0 ){
			if( fstat( fd, &statb ) != 0 ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_INFO) _("Check_files: stat of temp fd '%d' failed"), fd);
			}
			printable = Check_lpr_printable( s, fd, &statb, Format_JOB );
		}
		close( fd );
		if( printable > 0 ){
			lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
			memset(lp,0,sizeof(lp[0]));
			Check_max(&job->datafiles,1);
			job->datafiles.list[job->datafiles.count++] = (void *) lp;
			Set_str_value(lp,OPENNAME,tempfile);
			Set_str_value(lp,TRANSFERNAME,s);
			Clean_meta(s);
			Set_str_value(lp,"N",s);
			Set_flag_value(lp,COPIES,1);
			SNPRINTF(buffer,sizeof(buffer))"%c",Format_JOB);
			Set_str_value(lp,FORMAT,buffer);
			size = size + statb.st_size;
			Set_double_value(lp,SIZE,(double)(statb.st_size) );
			DEBUG2( "Check_files: printing '%s'", s );
		} else {
			DEBUG2( "Check_files: not printing '%s'", s );
		}
	}
	if( Copies_JOB ) size *= Copies_JOB;
	DEBUG2( "Check_files: %d files, size %0.0f", job->datafiles.count, size );
	return( size );
}

/***************************************************************************
 * int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
 * 1. Check to make sure it is a regular file.
 * 2. Check to make sure that it is not 'binary data' file
 * 3. If a text file,  check to see if it has some control characters
 *
 ***************************************************************************/

int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
{
    char buf[LINEBUFFER];
    int n, i, c;                /* Acme Integers, Inc. */
    int printable = 0;
	char *err = _("cannot print '%s': %s");

	if( Check_for_nonprintable_DYN == 0 ) return(1);
	/*
	 * Do an LSEEK on the file, i.e.- see to the start
	 * Ignore any error return
	 */
	lseek( fd, 0, SEEK_SET );
    if(!S_ISREG( statb->st_mode )) {
		DIEMSG(err, file,_("not a regular file"));
    } else if(statb->st_size == 0) {
		/* empty file */
		printable = -1;
    } else if ((n = read (fd, buf, sizeof(buf))) <= 0) {
        DIEMSG (err, file,_("cannot read it"));
    } else if (format != 'p' && format != 'f' ){
        printable = 1;
    } else if (is_exec ( buf, n)) {
        DIEMSG (err, file,_("executable program"));
    } else if (is_arch ( buf, n)) {
        DIEMSG (err, file,_("archive file"));
    } else {
        printable = 1;
		if( Min_printable_count_DYN && n > Min_printable_count_DYN ){
			n = Min_printable_count_DYN;
		}
		for (i = 0; printable && i < n; ++i) {
			c = cval(buf+i);
			/* we allow backspace, escape, ^D */
			if( !isprint( c ) && !isspace( c )
				&& c != 0x08 && c != 0x1B && c!= 0x04 ) printable = 0;
		}
		if( !printable ) DIEMSG (err, file,
			_("unprintable characters at start of file, check your LANG environment variable as well as the input file"));
    }
    return(printable);
}

/***************************************************************************
 * The is_exec and is_arch are system dependent functions which
 * check if a file is an executable or archive file, based on the
 * information in the header.  Note that most of the time we will end
 * up with a non-printable character in the first 100 characters,  so
 * this test is moot.
 *
 * I swear I must have been out of my mind when I put these tests in.
 * In fact,  why bother with them?  
 *
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 *   On review, I agree with myself. Sun Jan 31 06:36:28 PST 1999
 ***************************************************************************/

#if defined(HAVE_A_OUT_H) && !defined(_AIX41)
#include <a.out.h>
#endif

#ifdef HAVE_EXECHDR_H
#include <sys/exechdr.h>
#endif

/* this causes trouble, eg. on SunOS. */
#ifdef IS_NEXT
#  ifdef HAVE_SYS_LOADER_H
#    include <sys/loader.h>
#  endif
#  ifdef HAVE_NLIST_H
#    include <nlist.h>
#  endif
#  ifdef HAVE_STAB_H
#    include <stab.h>
#  endif
#  ifdef HAVE_RELOC_H
#   include <reloc.h>
#  endif
#endif /* IS_NEXT */

#if defined(HAVE_FILEHDR_H) && !defined(HAVE_A_OUT_H)
#include <filehdr.h>
#endif

#if defined(HAVE_AOUTHDR_H) && !defined(HAVE_A_OUT_H)
#include <aouthdr.h>
#endif

#ifdef HAVE_SGS_H
#include <sgs.h>
#endif

/***************************************************************************
 * I really don't want to know.  This alone tempts me to rip the code out
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/
#ifndef XYZZQ_
#define XYZZQ_ 1		/* ugh! antediluvian BSDism, I think */
#endif

#ifndef N_BADMAG
#  ifdef NMAGIC
#    define N_BADMAG(x) \
	   ((x).a_magic!=OMAGIC && (x).a_magic!=NMAGIC && (x).a_magic!=ZMAGIC)
#  else				/* no NMAGIC */
#    ifdef MAG_OVERLAY		/* AIX */
#      define N_BADMAG(x) (x.a_magic == MAG_OVERLAY)
#    endif				/* MAG_OVERLAY */
#  endif				/* NMAGIC */
#endif				/* N_BADMAG */

int is_exec( char *buf, int n)
{
    int i = 0;

#ifdef N_BADMAG		/* BSD, non-mips Ultrix */
#  ifdef HAVE_STRUCT_EXEC
    if (n >= (int)sizeof (struct exec)){
		i |= !(N_BADMAG ((*(struct exec *) buf)));
	}
#  else
    if (n >= (int)sizeof (struct aouthdr)){
		i |= !(N_BADMAG ((*(struct aouthdr *) buf)));
	}
#  endif
#endif

#ifdef ISCOFF		/* SVR4, mips Ultrix */
    if (n >= (int)sizeof (struct filehdr)){
		i |= (ISCOFF (((struct filehdr *) buf)->f_magic));
	}
#endif

#ifdef MH_MAGIC		/* NeXT */
    if (n >= (int)sizeof (struct mach_header)){
		i |= (((struct mach_header *) buf)->magic == MH_MAGIC);
	}
#endif

#ifdef IS_DATAGEN	/* Data General (forget it! ;) */
    {
		if( n > (int)sizeof (struct header)){
			i |= ISMAGIC (((struct header *)buff->magic_number));
		}
    }
#endif

    return (i);
}

#include <ar.h>

int is_arch(char *buf, int n)
{
	int i = 0;
#ifdef ARMAG
	if( n >= SARMAG ){
		i = !memcmp( buf, ARMAG, SARMAG);
	}
#endif				/* ARMAG */
    return(i);
}

void Dienoarg(int option)
{
	DIEMSG (_("option '%c' missing argument"), option);
}

/***************************************************************************
 * Check_int_dup (int option, int *value, char *arg)
 * 1.  check to see if value has been set
 * 2.  if not, then get integer value from arg
 ***************************************************************************/

void Check_int_dup (int option, int *value, char *arg, int maxvalue)
{
	char *convert;

	if (arg == 0) {
		Dienoarg (option);
	}
	convert = arg;
	*value = strtol( arg, &convert, 10 );
	if( *value < 0 || convert == arg || *convert ){
		DIEMSG (_("option %c parameter `%s` is not positive integer value"),
		        option, arg );
	}
	if( maxvalue > 0 && *value > maxvalue ){
		DIEMSG (_("option %c parameter `%s` is not integer value from 0 - %d"),
		        option, arg, maxvalue );
	}
}

/***************************************************************************
 * Check_str_dup(int option, char *value, char *arg)
 * 1.  check to see if value has been set
 * 2.  if not, then set it
 ***************************************************************************/

void Check_str_dup(int option, char **value, char *arg, int maxlen )
{
	if (arg == 0) {
		Dienoarg (option);
	}
	*value = arg;
}

/***************************************************************************
 * 1.  check to see if value has been set
 * 2.  if not, then set it
 ***************************************************************************/

void Check_dup(int option, int *value)
{
	*value = 1;
}

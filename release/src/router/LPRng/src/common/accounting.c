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
"$Id: accounting.c,v 1.1.1.1 2008/10/15 03:28:26 james26_jang Exp $";


#include "lp.h"
#include "accounting.h"
#include "getqueue.h"
#include "errorcodes.h"
#include "child.h"
#include "linksupport.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/

/*
 Do_accounting is called with:
   status = Do_accounting( 0, Accounting_start_DYN, job, Send_job_rw_timeout_DYN );
  OR
   status = Do_accounting( 0, Accounting_end_DYN, job, Send_job_rw_timeout_DYN );

 The general approach is:

  You are going to do accounting.  You either write accounting information to a
  file,  or to a program.   If the 'achk' flag is set then you write it to
  a program and then get back a 'proceed' indication.

  What you write is specified by the 'Accounting_start_DYN' (:as=) or 
  'Accounting_end_DYN' (:ae=) entries,  or the 'Accounting_file_DYN' (:af=)
  entries.

  By default,  the :as or :ae entries are a simple string and are interpreted as
  text to send.  If they are a path (starting with "/" or "|") then they are
  a program to run.  In this case we run the program with no input,  expecting
  that the necessary options will be passed on the command line.

  If the :as (or :ae) is a string,  then then :af is checked to see if it is a
  filter  ("|/..."), network port  ("host@port"), or file.  If it is a filter,
  then the program is run,  a network port then a connection is made to the remote
  host, and if a file, the file is opened.  In all three cases the :as or
  :ae string is written to the program, socket, or file respectively. 

  If we are starting the job and the :achk flag is set and have
  specified a filter (program) or host, then we now read the status
  back from the filter or remote host.  The connection is then
  closed.  If we are writing to a program then the exit status is
  first used to determine the disposition:
    JHOLD, JREMOVE, JABORT (or unknown) will cause the job to be
      held, removed, or aborted respectively.
    JSUCC will cause a line (or lines) to be read

  The first input line read from the remote program or host is used
  to determine job disposal.  If the line is blank or starts with
  ACCEPT then the job will be printed, HOLD will hold the job,  REMOVE
  will remove the job, and ABORT or a non-recognizable response will
  cause printing to be aborted.

*/

int Do_accounting( int end, char *command, struct job *job, int timeout )
{
	int n, err, len, tempfd;
	char msg[SMALLBUFFER];
	char *s, *t;
	struct line_list args;
	struct stat statb;


	Init_line_list(&args);
	msg[0] = 0;
	err = JSUCC;

	while( isspace(cval(command)) ) ++command;
	s = command;
	if( cval(s) == '|' ) ++s;
	Add_line_list(&args, s,0,0,0);
	Fix_dollars(&args, job, 1, Filter_options_DYN );
	s = args.list[0];
	DEBUG1("Do_accounting: command '%s', af '%s', expanded '%s'",
		command, Accounting_file_DYN, s );
	s = safeextend2(s,"\n",__FILE__,__LINE__);
	args.list[0] = s;

	tempfd = -1;

	if( (cval(command) == '|') || (cval(command) == '/') ){
		if( end == 0 && Accounting_check_DYN ){
			tempfd = Make_temp_fd( 0 );
		}
		err = Filter_file( -1, tempfd, "ACCOUNTING_FILTER",
			command, Filter_options_DYN, job, 0, 1 );
		if( tempfd > 0 && lseek(tempfd,0,SEEK_SET) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO)"Do_accounting: lseek tempfile failed");
		}
	} else if( !ISNULL(Accounting_file_DYN) ){
		if( (cval(Accounting_file_DYN) == '|') ){
			int fd = Make_temp_fd(0);
			if( Write_fd_str( fd, args.list[0] ) < 0 ){
				Errorcode= JFAIL;
				LOGERR_DIE(LOG_INFO)"Do_accounting: write to tempfile of '%s' failed", command);
			}
			if( fd > 0 && lseek(fd,0,SEEK_SET) == -1 ){
				Errorcode= JFAIL;
				LOGERR_DIE(LOG_INFO)"Do_accounting: seek of tempfile failed" );
			}
			if( end == 0 && Accounting_check_DYN ){
				tempfd = Make_temp_fd( 0 );
			}
			err = Filter_file( fd, tempfd, "ACCOUNTING_FILTER",
				Accounting_file_DYN, Filter_options_DYN, job, 0, 1 );
			if( tempfd > 0 && lseek(tempfd,0,SEEK_SET) == -1 ){
				Errorcode= JFAIL;
				LOGERR_DIE(LOG_INFO)"Do_accounting: seek of tempfile failed" );
			}
			close(fd);
		} else if( isalnum(cval(Accounting_file_DYN))
			&& safestrchr( Accounting_file_DYN, '%' ) ){
			/* now try to open a connection to a server */
			char *host = Accounting_file_DYN;
			
			DEBUG2("Do_accounting: connecting to '%s'",host);
			if( (tempfd = Link_open(host,timeout,0, 0 )) < 0 ){
				err = errno;
				Errorcode= JFAIL;
				LOGERR_DIE(LOG_INFO)
					_("connection to accounting server '%s' failed '%s'"),
					Accounting_file_DYN, Errormsg(err) );
			}
			DEBUG2("Setup_accounting: socket %d", tempfd );
			if( Write_fd_str( tempfd, args.list[0] ) < 0 ){
				Errorcode= JFAIL;
				LOGERR_DIE(LOG_INFO)"Do_accounting: write to '%s' failed", command);
			}
			shutdown(tempfd,1);
		} else {
			tempfd = Checkwrite( Accounting_file_DYN, &statb, 0, Create_files_DYN, 0 );
			if( !end ){
				tempfd = Trim_status_file( tempfd, Accounting_file_DYN, Max_accounting_file_size_DYN,
					Min_accounting_file_size_DYN );
			}
			DEBUG2("Do_accounting: fd %d", tempfd );
			if( tempfd > 0 ){
				if( Write_fd_str( tempfd, args.list[0] ) < 0 ){
					err = errno;
					Errorcode= JFAIL;
					LOGERR_DIE(LOG_INFO)"Do_accounting: write to '%s' failed", command);
				}
				close(tempfd); tempfd = -1;
			}
		}
	}

	if( tempfd > 0 && err == 0 && end == 0 && Accounting_check_DYN ){
		msg[0] = 0;
		len = 0;
		while( len < (int)(sizeof(msg)-1)
			&& (n = read(tempfd,msg+len,sizeof(msg)-1-len)) > 0 ){
			msg[len+n] = 0;
			DEBUG1("Do_accounting: read %d, '%s'", n, msg );
		}
		Free_line_list(&args);
		lowercase(msg);
		Split(&args,msg,Whitespace,0,0,0,0,0,0);
		err = JSUCC;
		if( args.count && (s = args.list[0]) ){
			if( (t = safestrchr(s,'\n')) ) *t = 0;
			SETSTATUS(job)"accounting filter replied with '%s'", s);
			if( *s == 0 || !safestrncasecmp( s, "accept", 6 ) ){
				err = JSUCC;
			} else if( !safestrncasecmp( s, "hold", 4 ) ){
				err = JHOLD;
			} else if( !safestrncasecmp( s, "remove", 6 ) ){
				err = JREMOVE;
			} else if( !safestrncasecmp( s, "fail", 4 ) ){
				err = JFAIL;
			} else {
				SNPRINTF( msg, sizeof(msg))
					"accounting check failed - status message '%s'", s );
				err = JABORT;
			}
		}
	}
	if( tempfd > 0 ) close(tempfd); tempfd = -1;
	Free_line_list(&args);
	DEBUG2("Do_accounting: status %s", Server_status(err) );
	return( err );
}

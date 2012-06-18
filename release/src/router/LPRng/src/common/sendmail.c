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
"$Id: sendmail.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";

#include "lp.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getqueue.h"
#include "sendmail.h"
#include "child.h"
/**** ENDINCLUDE ****/

/*
 * sendmail --- tell people about job completion
 * 1. fork a sendmail process
 * 2. if successful, send the good news
 * 3. if unsuccessful, send the bad news
 */

void Sendmail_to_user( int retval, struct job *job )
{
	char buffer[SMALLBUFFER], msg[SMALLBUFFER];
	int n, tempfd;
	char *id, *mailname, *opname, *s;

	/*
	 * check to see if the user really wanted
	 * "your file was printed ok" message
	 */
	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if(!id) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	mailname = Find_str_value(&job->info,MAILNAME,Value_sep);
	opname = Mail_operator_on_error_DYN;
	DEBUG2("Sendmail_to_user: user '%s', operator '%s', sendmail '%s'",
		mailname, opname, Sendmail_DYN );

/*
	We will let the sendmail script or program check for correct formats
		This allows the most hideous things to be done to mail messages
		but we know that this is a loophole
	if( !safestrchr( mailname,'%') && !safestrchr(mailname,'@') ) mailname = 0;
	if( !safestrchr( opname,'%') && !safestrchr(opname,'@') ) opname = 0;
*/
	if( retval == JSUCC ) opname = 0;
	if( Sendmail_DYN == 0 ) return;
	if( !Sendmail_to_user_DYN ) mailname = 0;
	if( mailname == 0 && opname == 0 ) return;

	tempfd = Make_temp_fd( 0 ); 

	DEBUG2("Sendmail_to_user: user '%s', operator '%s'", mailname, opname );

	msg[0] = 0;
	if( mailname ){
		SNPRINTF( msg, sizeof(msg)) "'%s'", mailname );
		SNPRINTF( buffer, sizeof(buffer)) "To: %s\n", mailname );
		if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;
	}
	if( opname ){
		n = safestrlen(msg);
		SNPRINTF( msg+n, sizeof(msg)-n) "%s'%s'",n?" and ":"", opname );
		SNPRINTF(buffer,sizeof(buffer))
		"%s: %s\n", mailname?"CC":"To", opname );
		if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;
	}
	setstatus( job, "sending mail to %s", msg );
	SNPRINTF(buffer,sizeof(buffer))
		"From: %s@%s\n",
		Mail_from_DYN ? Mail_from_DYN : Printer_DYN, FQDNHost_FQDN );
	if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;

	SNPRINTF(buffer,sizeof(buffer))
		"Subject: %s@%s job %s\n\n",
		Printer_DYN, FQDNHost_FQDN, id );
	if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;

	/* now do the message */
	SNPRINTF(buffer,sizeof(buffer))
		_("printer %s job %s"), Printer_DYN, id );
	if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;

	switch( retval) {
	case JSUCC:
		SNPRINTF(buffer,sizeof(buffer))
		_(" was successful.\n"));
		break;

	case JFAIL:
		SNPRINTF(buffer,sizeof(buffer))
		_(" failed, and retry count was exceeded.\n") );
		break;

	case JABORT:
		SNPRINTF(buffer,sizeof(buffer))
		_(" failed and could not be retried.\n") );
		break;

	default:
		SNPRINTF(buffer,sizeof(buffer))
		_(" died a horrible death.\n"));
		break;
	}
	if( Write_fd_str( tempfd, buffer ) < 0 ) goto wr_error;

	/*
	 * get the last status of the spooler
	 */
	if( (s = Get_file_image( Queue_status_file_DYN, Max_status_size_DYN )) ){
		if( Write_fd_str( tempfd, "\nStatus:\n\n" ) < 0 ||
			Write_fd_str( tempfd, s ) < 0 ) goto wr_error;
		if(s) free(s); s = 0;
	}

	if( (s = Get_file_image( Status_file_DYN, Max_status_size_DYN )) ){
		if( Write_fd_str( tempfd, "\nFilter Status:\n\n" ) < 0 ||
			Write_fd_str( tempfd, s ) < 0 ) goto wr_error;
		if(s) free(s); s = 0;
	}
	if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "Sendmail_to_user: seek failed");
	}
	n = Filter_file( tempfd, -1, "MAIL", Sendmail_DYN, 0, job, 0, 0 );
	if( n ){
		Errorcode = JABORT;
		LOGERR(LOG_ERR) "Sendmail_to_user: '%s' failed '%s'", Sendmail_DYN, Server_status(n) );
	}
	return;

 wr_error:
	Errorcode = JABORT;
	LOGERR_DIE(LOG_ERR) "Sendmail_to_user: write failed");
}

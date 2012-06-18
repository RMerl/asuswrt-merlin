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
"$Id: lpd_secure.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"
#include "user_auth.h"
#include "lpd_dispatch.h"
#include "getopt.h"
#include "getqueue.h"
#include "proctitle.h"
#include "permission.h"
#include "linksupport.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "lpd_rcvjob.h"
#include "child.h"
#include "globmatch.h"
#include "lpd_jobs.h"
#include "krb5_auth.h"
#include "lpd_secure.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Mon Apr 17 05:43:48 PDT 1995
 * 
 * The protocol used to send a secure job consists of the following
 * following:
 * 
 * \REQ_SECUREprintername C/F user authtype\n         - receive a command
 *              0           1   2  3        4
 * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
 *              0           1   2  3        4
 * 
 * The server will return an ACK, and then start the authentication
 * process.  See README.security for details.
 * 
 ***************************************************************************/

/*************************************************************************
 * Receive_secure() - receive a secure transfer
 *************************************************************************/
#ifdef ORIGINAL_DEBUG//JY@1020
int Receive_secure( int *sock, char *input )
{
	char *printername;
	char error[SMALLBUFFER];	/* error message */
	char *authtype;
	char *cf, *s;
	char *jobsize = 0;
	char *user = 0;
	int tempfd = -1;
	int ack, status, from_server;
	struct line_list args, header_info, info;
	struct stat statb;
	char *tempfile = 0;
	struct security *security = 0;

	Name = "RCVSEC";
	memset( error, 0, sizeof(error));
	ack = 0;
	status = 0;

	DEBUGF(DRECV1)("Receive_secure: input line '%s'", input );
	Init_line_list( &args );
	Init_line_list( &header_info );
	Init_line_list( &info );

	Split(&args,input+1,Whitespace,0,0,0,0,0,0);
	DEBUGFC(DRECV1)Dump_line_list("Receive_secure - input", &args);
	if( args.count != 5 && args.count != 4 ){
		SNPRINTF( error+1, sizeof(error)-1)
			_("bad command line '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}
	Check_max(&args,1);
	args.list[args.count] = 0;

	/*
     * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
     *              0           1   2  3        4
	 */
	printername = args.list[0];
	cf = args.list[1];
	user = args.list[2];	/* user is escape encoded */
	Unescape(user);
	authtype = args.list[3];
	Unescape(authtype);
	jobsize = args.list[4];

	setproctitle( "lpd %s '%s'", Name, printername );

	Perm_check.authtype = authtype;
	from_server = 0;
	if( *cf == 'F' ){
		from_server = 1;
	}

	/* set up the authentication support information */

	if( Is_clean_name( printername ) ){
		SNPRINTF( error+1, sizeof(error)-1)
			_("bad printer name '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}

	Set_DYN(&Printer_DYN,printername);

	if( Setup_printer( printername, error+1, sizeof(error)-1, 0 ) ){
		if( jobsize ){
			SNPRINTF( error+1, sizeof(error)-1)
				_("bad printer '%s'"), printername );
			ack = ACK_FAIL;	/* no retry, don't send again */
			status = JFAIL;
			goto error;
		}
	} else {
		int db, dbf;

		db = Debug;
		dbf = DbgFlag;
		s = Find_str_value(&Spool_control,DEBUG,Value_sep);
		if(!s) s = New_debug_DYN;
		Parse_debug( s, 0 );

		if( !(DRECVMASK & DbgFlag) ){
			Debug = db;
			DbgFlag = dbf;
		} else {
			int tdb, tdbf;
			tdb = Debug;
			tdbf = DbgFlag;
			Debug = db;
			DbgFlag = dbf;
			if( Log_file_DYN ){
				tempfd = Checkwrite( Log_file_DYN, &statb,0,0,0);
				if( tempfd > 0 && tempfd != 2 ){
					dup2(tempfd,2);
					close(tempfd);
				}
				tempfd = -1;
			}
			Debug = tdb;
			DbgFlag = tdbf;
			LOGDEBUG("Receive_secure: socket fd %d", *sock);
			Dump_line_list("Receive_secure - input", &args);
		}
		DEBUGF(DRECV1)("Receive_secure: debug '%s', Debug %d, DbgFlag 0x%x",
			s, Debug, DbgFlag );
	}

	if( !(security = Fix_receive_auth(authtype, &info)) ){
		SNPRINTF( error+1, sizeof(error)-1)
			_("unsupported authentication '%s'"), authtype );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}
	if( !security->server_receive ){
		SNPRINTF( error+1, sizeof(error)-1)
			_("no receive method supported for '%s'"), authtype );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}


	if( jobsize ){
		double read_len;
		read_len = strtod(jobsize,0);

		DEBUGF(DRECV2)("Receive_secure: spooling_disabled %d",
			Sp_disabled(&Spool_control) );
		if( Sp_disabled(&Spool_control) ){
			SNPRINTF( error+1, sizeof(error)-1)
				_("%s: spooling disabled"), Printer_DYN );
			ack = ACK_RETRY;	/* retry */
			status = JFAIL;
			goto error;
		}
		if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
			SNPRINTF( error+1, sizeof(error)-1)
				_("%s: job size %0.0f is larger than %d K"),
				Printer_DYN, read_len, Max_job_size_DYN );
			ack = ACK_RETRY;
			status = JFAIL;
			goto error;
		} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
			SNPRINTF( error+1, sizeof(error)-1)
				_("%s: insufficient file space"), Printer_DYN );
			ack = ACK_RETRY;
			status = JFAIL;
			goto error;
		}
	}

	tempfd = Make_temp_fd(&tempfile);
	close(tempfd); tempfd = -1;

	DEBUGF(DRECV1)("Receive_secure: sock %d, user '%s', jobsize '%s'",  
		*sock, user, jobsize );

	status = security->server_receive( sock,
		user, jobsize, from_server, authtype,
		&info,
		error+1, sizeof(error)-1,
		&header_info,
		security, tempfile );

 error:
	DEBUGF(DRECV1)("Receive_secure: status %d, ack %d, error '%s'",
		status, ack, error+1 );

	if( status ){
		if( ack == 0 ) ack = ACK_FAIL;
		error[0] = ack;
		DEBUGF(DRECV1)("Receive_secure: sending '%s'", error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN, error, safestrlen(error), 0 );
		Errorcode = JFAIL;
	}

	Free_line_list( &args );
	Free_line_list( &header_info );
	Free_line_list( &info );

	close( *sock ); *sock = -1;
	Remove_tempfiles();

	if( status == 0 && jobsize ){
		/* start a new server */
		DEBUGF(DRECV1)("Receive_secure: starting server");
		if( Server_queue_name_DYN ){
			Do_queue_jobs( Server_queue_name_DYN, 0 );
		} else {
			Do_queue_jobs( Printer_DYN, 0 );
		}
	}
	cleanup(0);
	return(0);
}
#endif

#ifdef ORIGINAL_DEBUG//JY@1020
int Do_secure_work( char *jobsize, int from_server,
	char *tempfile, struct line_list *header_info )
{
	int n, len, linecount = 0, done = 0, fd, status = 0;
	char *s, *t;
	char buffer[SMALLBUFFER];
	char error[SMALLBUFFER];
	struct stat statb;

	error[0] = 0;
	if( (fd = Checkread(tempfile,&statb)) < 0 ){ 
		status = JFAIL;
		SNPRINTF( error, sizeof(error))
			"Do_secure_work: reopen of '%s' failed - %s",
				tempfile, Errormsg(errno));
		goto error;
	}

	buffer[0] = 0;
	n = 0;
	done = 0;
	linecount = 0;

	while( !done && n < (int)sizeof(buffer)-1
		&& (len = read( fd, buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+len] = 0;
		DEBUGF(DRECV1)("Do_secure_work: read %d - '%s'", len, buffer );
		while( !done && (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			if( safestrlen(buffer) == 0 ){
				done = 1;
				break;
			}
			DEBUGF(DRECV1)("Do_secure_work: line [%d] '%s'", linecount, buffer );
			if( (t = strchr(buffer,'=')) ){
				*t++ = 0;
				Unescape(t);
				Set_str_value(header_info, buffer, t );
			} else {
				switch( linecount ){
					case 0:
						if( jobsize ){
							if( from_server ){
								Set_str_value(header_info,CLIENT,buffer);
							}
							done = 1;
						} else {
							Set_str_value(header_info,INPUT,buffer); break;
						}
						break;
					case 1:
						Set_str_value(header_info,CLIENT,buffer);
						done = 1;
						break;
				}
			}
			++linecount;
			memmove(buffer,s,safestrlen(s)+1);
			n = safestrlen(buffer);
		}
	}

	if( fd >= 0 ) close(fd); fd = -1;

	DEBUGFC(DRECV1)Dump_line_list("Do_secure_work - header", header_info );

	if( (status = Check_secure_perms( header_info, from_server, error, sizeof(error))) ){
		goto error;
	}


	buffer[0] = 0;
	if( jobsize ){
		if( (fd = Checkread(tempfile, &statb) ) < 0 ){
			status = JFAIL;
			SNPRINTF( error, sizeof(error))
				"Do_secure_work: reopen of '%s' for read failed - %s",
					tempfile, Errormsg(errno));
			goto error;
		}
		status = Scan_block_file( fd, error, sizeof(error), header_info );
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
			status = JFAIL;
			SNPRINTF( error, sizeof(error))
				"Do_secure_work: reopen of '%s' for write failed - %s",
					tempfile, Errormsg(errno));
			goto error;
		}
	} else {
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
			status = JFAIL;
			SNPRINTF( error, sizeof(error))
				"Do_secure_work: reopen of '%s' for write failed - %s",
					tempfile, Errormsg(errno));
			goto error;
		}
		if( (s = Find_str_value(header_info,INPUT,Value_sep)) ){
			Dispatch_input( &fd, s );
		}
	}

 error:

	if( fd >= 0 ) close(fd); fd = -1;
	DEBUGF(DRECV1)("Do_secure_work: status %d, tempfile '%s', error '%s'",
		status, tempfile, error );
	if( error[0] ){
		DEBUGF(DRECV1)("Do_secure_work: updating tempfile '%s', error '%s'",
			tempfile, error );
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO) "Do_secure_work: reopen of '%s' for write failed",
				tempfile );
		}
		Write_fd_str(fd,error);
		close(fd);
	}
	DEBUGF(DRECV1)("Do_secure_work: returning %d", status );
	return( status );
}
#endif

/***************************************************************************
 * void Fix_auth() - get the Use_auth_DYN value for the remote printer
 ***************************************************************************/

struct security *Fix_receive_auth( char *name, struct line_list *info )
{
	struct security *s;

	if( name == 0 ){
		if( Is_server ){
			name = Auth_forward_DYN;
		} else {
			name = Auth_DYN;
		}
	}

	for( s = SecuritySupported; s->name && Globmatch(s->name, name ); ++s );
	DEBUG1("Fix_receive_auth: name '%s' matches '%s'", name, s->name );
	if( s->name == 0 ){
		s = 0;
	} else {
		char buffer[64], *str;
		if( !(str = s->config_tag) ) str = s->name;
		SNPRINTF(buffer,sizeof(buffer))"%s_", str );
		Find_default_tags( info, Pc_var_list, buffer );
		Find_tags( info, &Config_line_list, buffer );
		Find_tags( info, &PC_entry_line_list, buffer );
		Expand_hash_values( info );
	}
	if(DEBUGL1)Dump_line_list("Fix_receive_auth: info", info );
	return(s);
}


int Check_secure_perms( struct line_list *options, int from_server,
	char *error, int errlen )
{
	/*
	 * line 1 - CLIENT=xxxx   - client authentication
	 * line 2 - SERVER=xxxx   - server authentication
	 * ...    - FROM=xxxx     - from
	 * line 3 - INPUT=\00x  - command line
	 */
	char *authfrom, *authuser;
	authfrom = Find_str_value(options,AUTHFROM,Value_sep);
	if( !authfrom ) authfrom = Find_str_value(options,FROM,Value_sep);
	authuser = Find_str_value(options,AUTHUSER,Value_sep);
	if( !from_server ){
		if( !authuser && authfrom ) authuser = authfrom;
	}
	if( !authuser ) authuser = Find_str_value(options,CLIENT,Value_sep);
	Set_str_value(options, AUTHTYPE, Perm_check.authtype );
	Set_str_value(options, AUTHFROM, authfrom );
	Set_str_value(options, AUTHUSER, authuser );
	Perm_check.authfrom = Find_str_value(options,AUTHFROM,Value_sep);
	Perm_check.authuser = authuser = Find_str_value(options,AUTHUSER,Value_sep);
	if( !authuser ){
		SNPRINTF( error, errlen) "Printer %s@%s: missing authentication client id",
			Printer_DYN,Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
		return( JABORT );
	}
	Perm_check.authca = Find_str_value(options,AUTHCA,Value_sep);
	DEBUGFC(DRECV1)Dump_line_list("Check_secure_perms - after",options);
	DEBUGFC(DRECV1)Dump_perm_check( "Check_secure_perms - checking", &Perm_check );
	return(0);
}

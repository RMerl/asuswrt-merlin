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
"$Id: sendjob.c,v 1.1.1.1 2008/10/15 03:28:27 james26_jang Exp $";


#include "lp.h"

#include "accounting.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getqueue.h"
#include "user_auth.h"
#include "linksupport.h"
#include "sendjob.h"
#include "sendauth.h"

/**** ENDINCLUDE ****/


/***************************************************************************
 * Commentary:
 * The protocol used to send a job to a remote RemoteHost_DYN consists of the
 * following:
 * 
 * Client                                   Server
 * \2RemotePrinter_DYNname\n - receive a job
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
 * 1. In order to abort the job transfer,  client sends \1
 * 2. Anything but a 0 ACK is an error indication
 * 
 * NB: some spoolers require that the data files be sent first.
 * The same transfer protocol is followed, but the data files are
 * send first,  followed by the control file.
 * 
 * The Send_job() routine will try to transfer a control file
 * to the remote RemoteHost_DYN.  It does so using the following algorithm.
 * 
 * 1.  makes a connection (connection timeout)
 * 2.  sends the \2RemotePrinter_DYN and gets ACK (transfer timeout)
 * 3.  sends the control file (transfer timeout)
 * 4.  sends the data files (transfer timeout)
 * 
 * int Send_job(
 * 	struct jobfile *job,	- control file
 * 	int connect_timeout_len,	- timeout on making connection
 * 	int connect_interval,	- interval between retries
 * 	int max_connect_interval - maximum connection interval
 * 	int transfer_timeout    - maximum time to send
 *
 * 	RETURNS: 0 if successful, non-zero if not
 **************************************************************************/

int Send_job( struct job *job, struct job *logjob,
	int connect_timeout_len, int connect_interval, int max_connect_interval,
	int transfer_timeout, char *final_filter )
{
	int sock = -1;		/* socket to use */
	char *id = 0, *s;
	char *real_host = 0, *save_host = 0;
	int status = 0, err, errcount = 0, n, len;
	char msg[SMALLBUFFER];
	char error[LARGEBUFFER];
	struct security *security = 0;
	struct line_list info;
 
	/* fix up the control file */
	Init_line_list(&info);
	if(DEBUGL1)Dump_job("Send_job- starting",job);
	Errorcode = 0;
	error[0] = 0;


	Set_str_value(&job->info,ERROR,0);
	Set_flag_value(&job->info,ERROR_TIME,0);
	/* send job to the LPD server for the RemotePrinter_DYN */

	id = Find_str_value( &job->info,IDENTIFIER,Value_sep);
	if( id == 0 ) id = Find_str_value( &job->info,TRANSFERNAME,Value_sep);
	DEBUG3("Send_job: '%s'->%s@%s,connect(timeout %d,interval %d)",
		id, RemotePrinter_DYN, RemoteHost_DYN,
			connect_timeout_len, connect_interval );

	/* determine authentication type to use */
	security = Fix_send_auth(0,&info,job, error, sizeof(error) );
	if( error[0] ){
		status = JFAIL;
		Set_str_value(&job->info,ERROR,error);
		Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		error[0] = 0;
		goto error;
	}
	if( final_filter && (security || Send_block_format_DYN) ){
		status = JABORT;
		Set_str_value(&job->info,ERROR,
			"Cannot have user filter with secure or block format transfer");
		Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		goto error;
	}

	SETSTATUS(logjob)
	"sending job '%s' to %s@%s",
		id, RemotePrinter_DYN, RemoteHost_DYN );

 retry_connect:
	error[0] = 0;
	Set_str_value(&job->info,ERROR,0);
	Set_flag_value(&job->info,ERROR_TIME,0);
	SETSTATUS(logjob) "connecting to '%s', attempt %d",
		RemoteHost_DYN, errcount+1 );
	if( (Is_server || errcount) && Network_connect_grace_DYN > 0 ){
		plp_sleep( Network_connect_grace_DYN );
	}

	errno = 0;

#ifdef ORIGINAL_DEBUG//JY@1020
	sock = Link_open_list( RemoteHost_DYN,
		&real_host, connect_timeout_len, 0, Unix_socket_path_DYN );
#endif

	err = errno;
	DEBUG4("Send_job: socket %d", sock );
	if( sock < 0 ){
		++errcount;
		status = LINK_OPEN_FAIL;
		msg[0] = 0;
		if( !Is_server && err ){
			SNPRINTF( msg, sizeof(msg))
			"\nMake sure the remote host supports the LPD protocol");
			if( geteuid() && getuid() ){
				int v = safestrlen(msg);
				SNPRINTF( msg+v, sizeof(msg)-v)
				"\nand accepts connections from this host and from non-privileged (>1023) ports");
			}
		}
		SNPRINTF( error, sizeof(error)-2)
			"cannot open connection to %s - %s%s", RemoteHost_DYN,
				err?Errormsg(err):"bad or missing hostname?", msg );
		if( Is_server && Retry_NOLINK_DYN ){
			if( connect_interval > 0 ){
				n = (connect_interval * (1 << (errcount - 1)));
				if( max_connect_interval && n > max_connect_interval ){
					n = max_connect_interval;
				}
				if( n > 0 ){
					SETSTATUS(logjob)
					_("sleeping %d secs before retry, starting sleep"),n );
					plp_sleep( n );
				}
			}
			goto retry_connect;
		}
		SETSTATUS(logjob) error );
		goto error;
	}
	save_host = safestrdup(RemoteHost_DYN,__FILE__,__LINE__);
	Set_DYN(&RemoteHost_DYN, real_host );
	if( real_host ) free( real_host );
	SETSTATUS(logjob) "connected to '%s'", RemoteHost_DYN );

	if( security && security->client_connect ){
		status = security->client_connect( job, &sock,
			transfer_timeout,
			error, sizeof(error),
			security, &info );
		if( status ) goto error;
	}
	if( security && security->client_send ){
		status = Send_auth_transfer( &sock, transfer_timeout,
			job, logjob, error, sizeof(error)-1, 0, security, &info );
	} else if( Send_block_format_DYN ){
		status = Send_block( &sock, job, logjob, transfer_timeout );
	} else {
		status = Send_normal( &sock, job, logjob, transfer_timeout, 0, final_filter );
	}
	DEBUG2("Send_job: after sending, status %d, error '%s'",
		status, error );
	if( status ) goto error;

	SETSTATUS(logjob) "done job '%s' transfer to %s@%s",
		id, RemotePrinter_DYN, RemoteHost_DYN );

 error:

	if( sock >= 0 ) sock = Shutdown_or_close(sock);
	if( status ){
		if( (s = Find_str_value(&job->info,ERROR,Value_sep )) ){
			SETSTATUS(logjob) "job '%s' transfer to %s@%s failed\n  %s",
				id, RemotePrinter_DYN, RemoteHost_DYN, s );
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		}
		DEBUG2("Send_job: sock is %d", sock);
		if( sock >= 0 ){
			len = 0;
			msg[0] = 0;
			n = 0;
			while( len < (int)sizeof(msg)-1
				&& (n = read(sock,msg+len,sizeof(msg)-len-1)) > 0 ){
				msg[len+n] = 0;
				DEBUG2("Send_job: read %d, '%s'", n, msg);
				while( (s = safestrchr(msg,'\n')) ){
					*s++ = 0;
					SETSTATUS(logjob) "error msg: '%s'", msg );
					memmove(msg,s,safestrlen(s)+1);
				}
				len = safestrlen(msg);
			}
			DEBUG2("Send_job: read %d, '%s'", n, msg);
			if( len ) SETSTATUS(logjob) "error msg: '%s'", msg );
		}
	}
	if( sock >= 0 ) close(sock); sock = -1;
	if( save_host ){
		Set_DYN(&RemoteHost_DYN,save_host);
		free(save_host); save_host = 0;
	}
	Free_line_list(&info);
	return( status );
}

/***************************************************************************
 * int Send_normal(
 * 	int sock,					- socket to use
 * 	struct job *job, struct job *logjob,	- control file
 * 	int transfer_timeout,		- transfer timeout
 * 	)						- acknowlegement status
 * 
 *  1. send the \2RemotePrinter_DYN\n string to the remote RemoteHost_DYN, wait for an ACK
 *  
 *  2. if control file first, send the control file: 
 *         send \3count cfname\n
 *         get back <0> ack
 *         send 'count' file bytes
 *         send <0> term
 *         get back <0> ack
 *  3. for each data file
 *         send the \4count dfname\n
 *             Note: count is 0 if file is filter
 *         get back <0> ack
 *         send 'count' file bytes
 *             Close socket and finish if filter
 *         send <0> term
 *         get back <0> ack
 *   4. If control file last, send the control file as in step 2.
 *       
 * 
 * If the block_fd parameter is non-zero, we write out the
 * control and data information to a file instead.
 * 
 ***************************************************************************/

int Send_normal( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter )
{
	char status = 0, *id, *transfername;
	char line[SMALLBUFFER];
	char error[SMALLBUFFER];
	int ack;

	DEBUG3("Send_normal: send_data_first %d, sock %d, block_fd %d",
		Send_data_first_DYN, *sock, block_fd );

	id = Find_str_value(&job->info,"A",Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	
	if( !block_fd ){
		SETSTATUS(logjob) "requesting printer %s@%s",
			RemotePrinter_DYN, RemoteHost_DYN );
		SNPRINTF( line, sizeof(line)) "%c%s\n",
			REQ_RECV, RemotePrinter_DYN );
		ack = 0;
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			line, safestrlen(line), &ack ) )){
			char *v;
			if( (v = safestrchr(line,'\n')) ) *v = 0;
			if( ack ){
				SNPRINTF(error,sizeof(error))
					"error '%s' with ack '%s'\n  sending str '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), line,
					RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				SNPRINTF(error,sizeof(error))
					"error '%s'\n  sending str '%s' to %s@%s",
					Link_err_str(status), line,
					RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			return(status);
		}
	}

	if( !block_fd && Send_data_first_DYN ){
		status = Send_data_files( sock, job, logjob, transfer_timeout, block_fd, final_filter );
		if( !status ) status = Send_control(
			sock, job, logjob, transfer_timeout, block_fd );
	} else {
		status = Send_control( sock, job, logjob, transfer_timeout, block_fd );
		if( !status ) status = Send_data_files(
			sock, job, logjob, transfer_timeout, block_fd, final_filter );
	}
	return(status);
}

int Send_control( int *sock, struct job *job, struct job *logjob, int transfer_timeout,
	int block_fd )
{
	char msg[SMALLBUFFER];
	char error[SMALLBUFFER];
	int status = 0, size, ack, err;
	char *cf = 0, *transfername = 0, *s;
	/*
	 * get the total length of the control file
	 */

	if( !(cf = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep)) ){
		s = Find_str_value(&job->info,OPENNAME,Value_sep);
		if( !s ) s = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		s = Get_file_image( s, 0 );
		Set_str_value(&job->info, CF_OUT_IMAGE, s );
		if( s ) free(s); s = 0;
		cf = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep);
	}
	size = safestrlen(cf);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	DEBUG3( "Send_control: '%s' is %d bytes, sock %d, block_fd %d, cf '%s'",
		transfername, size, *sock, block_fd, cf );
	if( !block_fd ){
		SETSTATUS(logjob) "sending control file '%s' to %s@%s",
		transfername, RemotePrinter_DYN, RemoteHost_DYN );
	}

	ack = 0;
	errno = 0;
	error[0] = 0;
	SNPRINTF( msg, sizeof(msg)) "%c%d %s\n",
		CONTROL_FILE, size, transfername);
	if( !block_fd ){
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			msg, safestrlen(msg), &ack )) ){
			if( (s = safestrchr(msg,'\n')) ) *s = 0;
			if( ack ){
				SNPRINTF(error,sizeof(error))
				"error '%s' with ack '%s'\n  sending str '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), msg,
				RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				SNPRINTF(error,sizeof(error))
				"error '%s'\n  sending str '%s' to %s@%s",
				Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			status = JFAIL;
			goto error;
		}
	} else {
		if( Write_fd_str( block_fd, msg ) < 0 ){
			goto write_error;
		}
	}

	/*
	 * send the control file
	 */
	errno = 0;
	if( block_fd == 0 ){
		/* we include the 0 at the end */
		ack = 0;
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			cf,size+1,&ack )) ){
			if( ack ){
				SNPRINTF(error,sizeof(error))
				"error '%s' with ack '%s'\n  sending control file '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), transfername,
				RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				SNPRINTF(error,sizeof(error))
					"error '%s'\n  sending control file '%s' to %s@%s",
					Link_err_str(status), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			status = JFAIL;
			goto error;
		}
		DEBUG3( "Send_control: control file '%s' sent", transfername );
		SETSTATUS(logjob) "completed sending '%s' to %s@%s",
			transfername, RemotePrinter_DYN, RemoteHost_DYN );
	} else {
		if( Write_fd_str( block_fd, cf ) < 0 ){
			goto write_error;
		}
	}
	status = 0;
	goto error;

 write_error:
	err = errno;
	SNPRINTF(error,sizeof(error))
		"job '%s' write to temporary file failed '%s'",
		transfername, Errormsg( err ) );
	Set_str_value(&job->info,ERROR,error);
	Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
	status = JFAIL;
 error:
	return(status);
}


int Send_data_files( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter )
{
	int count, fd, err, status = 0, ack;
	double size, sendsize;
	struct line_list *lp;
	char *openname, *transfername, *id, *s;
	char msg[SMALLBUFFER];
	char error[SMALLBUFFER];
	struct stat statb;

	DEBUG3( "Send_data_files: data file count '%d'", job->datafiles.count );
	id = Find_str_value(&job->info,"identification",Value_sep);
	if( id == 0 ) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	for( count = 0; count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		if(DEBUGL3)Dump_line_list("Send_data_files - entries",lp);
		openname = Find_str_value(lp,OPENNAME,Value_sep);
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		DEBUG3("Send_data_files: opening file '%s'", openname );

		/*
		 * open file as user; we should be running as user
		 */
		sendsize = size = 0;
		if( openname == 0 ){
			openname = "(STDIN)";
			fd = 0;
			size = 0;
			sendsize = Fake_large_file_DYN * 1024;
		} else {
			fd = Checkread( openname, &statb );
			sendsize = size = statb.st_size;
			if( statb.st_size == 0 ){
				SNPRINTF(error,sizeof(error))
				"zero length file '%s'", transfername );
				status = JABORT;
				Set_str_value(&job->info,ERROR,error);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				goto error;
			}
		}
		if( count == job->datafiles.count -1 && final_filter ){
			size = 0;
			sendsize = Fake_large_file_DYN * 1024;
		}
		err = errno;
		if( fd < 0 ){
			status = JFAIL;
			SNPRINTF(error,sizeof(error))
				"cannot open '%s' - '%s'", openname, Errormsg(err) );
			Set_str_value(&job->info,ERROR,error);
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			goto error;
		}

		DEBUG3( "Send_data_files: openname '%s', fd %d, size %0.0f",
			openname, fd, size );
		/*
		 * send the data file name line
		 */
		SNPRINTF( msg, sizeof(msg)) "%c%0.0f %s\n",
				DATA_FILE, size, transfername );
		if( block_fd == 0 ){
			SETSTATUS(logjob) "sending data file '%s' to %s@%s", transfername,
				RemotePrinter_DYN, RemoteHost_DYN );
			DEBUG3("Send_data_files: data file msg '%s'", msg );
			errno = 0;
			if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
				msg, safestrlen(msg), &ack )) ){
				if( (s = safestrchr(msg,'\n')) ) *s = 0;
				if( ack ){
					SNPRINTF(error,sizeof(error))
					"error '%s' with ack '%s'\n  sending str '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), msg,
					RemotePrinter_DYN, RemoteHost_DYN );
				} else {
					SNPRINTF(error,sizeof(error))
					"error '%s'\n  sending str '%s' to %s@%s",
					Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
				}
				Set_str_value(&job->info,ERROR,error);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				goto error;
			}

			/*
			 * send the data files content
			 */
			DEBUG3("Send_data_files: transfering '%s', fd %d", openname, fd );
			ack = 0;
			if( count == job->datafiles.count-1 && final_filter ){
				status = Filter_file( fd, *sock, "UserFilter",
					final_filter, Filter_options_DYN, job, 0, 1 );
				DEBUG3("Send_data_files: final_filter '%s' status %d", final_filter, status );
				close(fd); fd = 0;
			} else {
				status = Link_copy( RemoteHost_DYN, sock, 0, transfer_timeout,
						openname, fd, size );
			}
			/* special case - cannot read error code from other end */
			if( fd == 0 ){
				close(*sock);
				*sock = -1;
			}
			if( status 
				|| ( fd !=0 && (status = Link_send( RemoteHost_DYN,sock,
					transfer_timeout,"",1,&ack )) ) ){
				if( ack ){
					SNPRINTF(error,sizeof(error))
					"error '%s' with ack '%s'\n  sending data file '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
				} else {
					SNPRINTF(error,sizeof(error))
					"error '%s'\n  sending data file '%s' to %s@%s",
					Link_err_str(status), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
				}
				Set_str_value(&job->info,ERROR,error);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				goto error;
			}
			SETSTATUS(logjob) "completed sending '%s' to %s@%s",
				transfername, RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			double total;
			int len;

			if( Write_fd_str( block_fd, msg ) < 0 ){
				goto write_error;
			}
			/* now we need to read the file and transfer it */
			total = 0;
			while( total < size && (len = read(fd, msg, sizeof(msg)) ) > 0 ){
				if( write( block_fd, msg, len ) < 0 ){
					goto write_error;
				}
				total += len;
			}
			if( total != size ){
				SNPRINTF(error,sizeof(error))
					"job '%s' did not copy all of '%s'",
					id, transfername );
				status = JFAIL;
				Set_str_value(&job->info,ERROR,error);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				goto error;
			}
		}
		close(fd); fd = -1;
	}
	goto error;

 write_error:
	err = errno;
	SNPRINTF(error,sizeof(error))
		"job '%s' write to temporary file failed '%s'",
		id, Errormsg( err ) );
	Set_str_value(&job->info,ERROR,error);
	Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
	status = JFAIL;

 error:
	return(status);
}

/***************************************************************************
 * int Send_block(
 * 	char *RemoteHost_DYN,				- RemoteHost_DYN name
 * 	char *RemotePrinter_DYN,			- RemotePrinter_DYN name
 * 	char *dpathname *dpath  - spool directory pathname
 * 	int *sock,					- socket to use
 * 	struct job *job, struct job *logjob,	- control file
 * 	int transfer_timeout,		- transfer timeout
 * 	)						- acknowlegement status
 * 
 *  1. Get a temporary file
 *  2. Generate the compressed data files - this has the format
 *       \3count cfname\n
 *       [count control file bytes]
 *       \4count dfname\n
 *       [count data file bytes]
 * 
 *  3. send the \6RemotePrinter_DYN size\n
 *     string to the remote RemoteHost_DYN, wait for an ACK
 *  
 *  4. send the compressed data files
 *       wait for an ACK
 * 
 ***************************************************************************/

int Send_block( int *sock, struct job *job, struct job *logjob, int transfer_timeout )
{
	int tempfd;			/* temp file for data transfer */
	char msg[SMALLBUFFER];	/* buffer */
	char error[SMALLBUFFER];	/* buffer */
	struct stat statb;
	double size;				/* ACME! The best... */
	int status = 0;				/* job status */
	int ack;
	char *id, *transfername, *tempfile;

	error[0] = 0;
	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( id == 0 ) id = transfername;

	tempfd = Make_temp_fd( &tempfile );
	DEBUG1("Send_block: sending '%s' to '%s'", id, tempfile );

	status = Send_normal( &tempfd, job, logjob, transfer_timeout, tempfd, 0 );

	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( id == 0 ) id = transfername;

	DEBUG1("Send_block: sendnormal of '%s' returned '%s'", id, Server_status(status) );
	if( status ) return( status );

	/* rewind the file */
	if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Send_files: lseek tempfd failed" );
	}
	/* now we have the copy, we need to send the control message */
	if( fstat( tempfd, &statb ) ){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO) "Send_files: fstat tempfd failed" );
	}
	size = statb.st_size;

	/* now we know the size */
	DEBUG3("Send_block: size %0.0f", size );
	SETSTATUS(logjob) "sending job '%s' to %s@%s, block transfer",
		id, RemotePrinter_DYN, RemoteHost_DYN );
	SNPRINTF( msg, sizeof(msg)) "%c%s %0.0f\n",
		REQ_BLOCK, RemotePrinter_DYN, size );
	DEBUG3("Send_block: sending '%s'", msg );
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		msg, safestrlen(msg), &ack );
	DEBUG3("Send_block: status '%s'", Link_err_str(status) );
	if( status ){
		char *v;
		if( (v = safestrchr(msg,'\n')) ) *v = 0;
		if( ack ){
			SNPRINTF(error,sizeof(error))
			"error '%s' with ack '%s'\n  sending str '%s' to %s@%s",
			Link_err_str(status), Ack_err_str(ack), msg,
			RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			SNPRINTF(error,sizeof(error))
			"error '%s'\n  sending str '%s' to %s@%s",
			Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
		}
		Set_str_value(&job->info,ERROR,error);
		Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		return(status);
	}

	/* now we send the data file, followed by a 0 */
	DEBUG3("Send_block: sending data" );
	ack = 0;
	status = Link_copy( RemoteHost_DYN, sock, 0, transfer_timeout,
		transfername, tempfd, size );
	DEBUG3("Send_block: status '%s'", Link_err_str(status) );
	if( status == 0 ){
		status = Link_send( RemoteHost_DYN,sock,transfer_timeout,"",1,&ack );
		DEBUG3("Send_block: ack status '%s'", Link_err_str(status) );
	}
	if( status ){
		char *v;
		if( (v = safestrchr(msg,'\n')) ) *v = 0;
		if( ack ){
			SNPRINTF(error,sizeof(error))
				"error '%s' with ack '%s'\n  sending block file '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), id,
				RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			SNPRINTF(error,sizeof(error))
				"error '%s'\n  sending block file '%s' to %s@%s",
				Link_err_str(status), id, RemotePrinter_DYN, RemoteHost_DYN );
		}
		Set_str_value(&job->info,ERROR,error);
		Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		return(status);
	} else {
		SETSTATUS(logjob) "completed sending '%s' to %s@%s",
			id, RemotePrinter_DYN, RemoteHost_DYN );
	}
	close( tempfd ); tempfd = -1;
	return( status );
}

